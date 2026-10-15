import test from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { createHash } from 'node:crypto';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { fileURLToPath } from 'node:url';
import { mkdtemp, readFile, writeFile, rm } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { loadReviewedSingleSource, prepareReviewedSingleRgba, prepareSingleSpriteFile, validateSingleSpriteConfig } from './prepare_reviewed_single_sprite.mjs';

const sharp = createRequire(import.meta.url)('sharp');
const hash = bytes => createHash('sha256').update(bytes).digest('hex');
const execute = promisify(execFile);
const clone = value => JSON.parse(JSON.stringify(value));

function fixture(size = 8, outputFrame = 3) {
  const data = Buffer.alloc(size * size * 4);
  for (let y = 0; y < size; y++) for (let x = 0; x < size; x++) {
    const interior = x > 0 && y > 0 && x < size - 1 && y < size - 1;
    data.set([20 + x % 200, 30 + y % 200, 90, interior ? (x === 1 ? 128 : 220) : 127], (y * size + x) * 4);
  }
  const source = { path: 'reviewed single source.png', sha256: hash(Buffer.from('reviewed-encoded-source')), width: size, height: size, channels: 4, data };
  const config = { source: { path: source.path, sha256: source.sha256 }, expectedSourceSize: size, normalizedCanvasSize: 512, outputFrame };
  return { source, config };
}

function cellBytes(output, frame) {
  const data = Buffer.alloc(512 * 512 * 4);
  for (let row = 0; row < 512; row++) {
    const offset = ((Math.floor(frame / 4) * 512 + row) * output.width + frame % 4 * 512) * 4;
    output.data.copy(data, row * 512 * 4, offset, offset + 512 * 4);
  }
  return data;
}

function referenceNormalize(source) {
  const result = Buffer.alloc(512 * 512 * 4);
  for (let y = 0; y < 512; y++) for (let x = 0; x < 512; x++) {
    const sx = Math.floor((x + 0.5) * source.width / 512);
    const sy = Math.floor((y + 0.5) * source.height / 512);
    const offset = (sy * source.width + sx) * 4;
    if (source.data[offset + 3] < 128) continue;
    result.set([source.data[offset], source.data[offset + 1], source.data[offset + 2], 255], (y * 512 + x) * 4);
  }
  return result;
}

test('one populated cell, seven transparent cells, exact provenance and source unchanged', () => {
  const { source, config } = fixture();
  const before = Buffer.from(source.data);
  const result = prepareReviewedSingleRgba(config, source);
  assert.deepEqual([result.width, result.height], [2048, 1024]);
  assert.deepEqual(cellBytes(result, 3), referenceNormalize(source));
  for (let frame = 0; frame < 8; frame++) if (frame !== 3) assert.deepEqual(cellBytes(result, frame), Buffer.alloc(512 * 512 * 4));
  assert.deepEqual(source.data, before);
  assert.equal(result.manifest.nonemptyFrameCount, 1);
  assert.deepEqual(result.manifest.populatedFrames, [3]);
  assert.deepEqual(result.manifest.emptyFrames, [0, 1, 2, 4, 5, 6, 7]);
  assert.equal(result.manifest.source.sha256, source.sha256);
  assert.equal(result.manifest.source.rawRgbaSha256, hash(before));
  assert.equal(result.manifest.outputRawRgbaSha256, hash(result.data));
  assert.deepEqual(result.manifest.normalization.sourceRegion, [0, 0, 8, 8]);
  assert.deepEqual(result.manifest.normalization.uniformScale, { numerator: 512, denominator: 8, value: 64 });
  assert.match(result.manifest.acceptanceLimits, /not an eight-frame animation/);
  assert.match(result.manifest.acceptanceLimits, /explicit human frameMap selection/);
});

test('all eight explicitly named output cells are honored', () => {
  for (let frame = 0; frame < 8; frame++) {
    const { source, config } = fixture(8, frame);
    const result = prepareReviewedSingleRgba(config, source);
    assert.deepEqual(cellBytes(result, frame), referenceNormalize(source));
    assert.deepEqual(result.manifest.outputRegion, [frame % 4 * 512, Math.floor(frame / 4) * 512, 512, 512]);
  }
});

test('1254 to 512 is a single full-canvas nearest ratio, not an actor-bounds fit', () => {
  const { source, config } = fixture(1254, 6);
  const before = hash(source.data);
  const result = prepareReviewedSingleRgba(config, source);
  assert.deepEqual(cellBytes(result, 6), referenceNormalize(source));
  assert.deepEqual(result.manifest.normalization.uniformScale, { numerator: 512, denominator: 1254, value: 512 / 1254 });
  assert.equal(hash(source.data), before);
});

test('512 canvas preserves position and RGB; threshold is hard and happens before placement', () => {
  const { source, config } = fixture(512, 0);
  source.data.fill(0);
  for (const [x, alpha] of [[30, 127], [31, 128], [32, 129], [33, 255]]) {
    source.data.set([44, 55, 66, alpha], (40 * 512 + x) * 4);
  }
  const result = prepareReviewedSingleRgba(config, source);
  const cell = cellBytes(result, 0);
  assert.deepEqual(cell.subarray((40 * 512 + 30) * 4, (40 * 512 + 30) * 4 + 4), Buffer.from([0, 0, 0, 0]));
  for (const x of [31, 32, 33]) assert.deepEqual(cell.subarray((40 * 512 + x) * 4, (40 * 512 + x) * 4 + 4), Buffer.from([44, 55, 66, 255]));
  assert.deepEqual(result.manifest.source.visibleBoundsExclusive, [31, 40, 34, 41]);
  assert.deepEqual(result.manifest.normalization.visibleBoundsExclusive, [31, 40, 34, 41]);
});

test('different actor bounds do not alter the full-canvas coordinate mapping', () => {
  const { source, config } = fixture(1024, 0);
  source.data.fill(0);
  source.data.set([101, 102, 103, 128], (501 * 1024 + 501) * 4);
  const one = prepareReviewedSingleRgba(config, source);
  const wider = { ...source, data: Buffer.from(source.data) };
  wider.data.set([11, 12, 13, 255], (101 * 1024 + 101) * 4);
  const two = prepareReviewedSingleRgba(config, wider);
  const offset = (250 * 512 + 250) * 4;
  assert.deepEqual(cellBytes(one, 0).subarray(offset, offset + 4), Buffer.from([101, 102, 103, 255]));
  assert.deepEqual(cellBytes(two, 0).subarray(offset, offset + 4), Buffer.from([101, 102, 103, 255]));
  assert.deepEqual(one.manifest.normalization.uniformScale, two.manifest.normalization.uniformScale);
});

test('visible alpha 128 on any source edge is refused, while alpha 127 is discarded', () => {
  for (const [x, y] of [[0, 3], [7, 3], [3, 0], [3, 7]]) {
    const { source, config } = fixture();
    source.data[(y * 8 + x) * 4 + 3] = 128;
    assert.throws(() => prepareReviewedSingleRgba(config, source), /touch a canvas edge/);
  }
  const { source, config } = fixture();
  assert.doesNotThrow(() => prepareReviewedSingleRgba(config, source));
});

test('empty or subthreshold source is refused even if RGB is nonzero', () => {
  for (const alpha of [0, 127]) {
    const { source, config } = fixture();
    for (let offset = 3; offset < source.data.length; offset += 4) source.data[offset] = alpha;
    assert.throws(() => prepareReviewedSingleRgba(config, source), /no visible pixels/);
  }
});

test('nearest sampling that loses the only source pixel is refused without inserting pixels', () => {
  const { source, config } = fixture(1024, 0);
  source.data.fill(0);
  source.data.set([12, 34, 56, 255], (100 * 1024 + 100) * 4);
  assert.throws(() => prepareReviewedSingleRgba(config, source), /leaves an empty frame/);
});

test('decoded dimensions, square aspect, channels, buffer type, byte count and reviewed hash are enforced', () => {
  for (const change of [
    item => item.width++, item => item.height++, item => item.channels = 3,
    item => item.data = new Uint8Array(item.data), item => item.data = item.data.subarray(1),
    item => item.sha256 = hash(Buffer.from('wrong-file')),
  ]) {
    const { source, config } = fixture();
    change(source);
    assert.throws(() => prepareReviewedSingleRgba(config, source));
  }
});

test('unsupported operations and invalid schema, local paths, hashes, dimensions and output indices are refused', () => {
  const { config } = fixture();
  for (const change of [
    item => item.source = null, item => item.source.path = '', item => item.source.path = 'https://example.test/source.png',
    item => item.source.path = 'bad\0.png', item => item.source.sha256 = 'abc', item => item.source.sha256 = 5,
    item => item.source.rotate = 90, item => item.expectedSourceSize = [8, 8], item => item.expectedSourceSize = 2,
    item => item.expectedSourceSize = 8.5, item => item.expectedSourceSize = 4097,
    item => item.normalizedCanvasSize = 256, item => item.normalizedCanvasSize = '512',
    item => item.outputFrame = -1, item => item.outputFrame = 8, item => item.outputFrame = 0.5,
    item => item.outputFrame = '3', item => delete item.outputFrame,
    item => item.bboxScale = true, item => item.scaleX = 0.5, item => item.anchor = [256, 336],
  ]) {
    const invalid = clone(config);
    change(invalid);
    assert.throws(() => validateSingleSpriteConfig(invalid));
  }
  for (const invalid of [null, [], 'config']) assert.throws(() => validateSingleSpriteConfig(invalid));
});

async function withFileFixture(callback, size = 8) {
  const directory = await mkdtemp(path.join(os.tmpdir(), 'immortal-reviewed-single-'));
  try {
    const { source, config } = fixture(size);
    const bytes = await sharp(source.data, { raw: { width: size, height: size, channels: 4 } }).png().toBuffer();
    const sourceFile = path.join(directory, source.path);
    await writeFile(sourceFile, bytes, { flag: 'wx' });
    config.source.sha256 = hash(bytes);
    const configFile = path.join(directory, 'reviewed.json');
    await writeFile(configFile, JSON.stringify(config), { flag: 'wx' });
    await callback({ directory, source, config, configFile, sourceFile, originalBytes: bytes });
  } finally {
    // This directory is created here by mkdtemp; no project/source tree is removed.
    await rm(directory, { recursive: true, force: false });
  }
}

test('PNG round trip has one 512 cell, seven empty cells and complete file provenance', async () => {
  await withFileFixture(async ({ directory, config, configFile, sourceFile, originalBytes, source }) => {
    const outputFile = path.join(directory, 'single prepared.png');
    const report = await prepareSingleSpriteFile(configFile, outputFile);
    const png = await readFile(outputFile);
    const { data, info } = await sharp(png).raw().toBuffer({ resolveWithObject: true });
    assert.deepEqual([info.width, info.height, info.channels], [2048, 1024, 4]);
    assert.deepEqual(cellBytes({ data, width: info.width }, 3), referenceNormalize(source));
    for (let frame = 0; frame < 8; frame++) if (frame !== 3) assert.deepEqual(cellBytes({ data, width: info.width }, frame), Buffer.alloc(512 * 512 * 4));
    assert.equal(report.source.sha256, config.source.sha256);
    assert.equal(report.source.path, sourceFile);
    assert.equal(report.configSha256, hash(await readFile(configFile)));
    assert.equal(report.outputSha256, hash(png));
    assert.deepEqual(await readFile(sourceFile), originalBytes);
  }, 1254);
});

test('exclusive creation protects existing revision and original source bytes', async () => {
  await withFileFixture(async ({ directory, configFile, sourceFile, originalBytes }) => {
    const existing = path.join(directory, 'existing.png');
    const sentinel = Buffer.from('preserve-existing-revision');
    await writeFile(existing, sentinel, { flag: 'wx' });
    await assert.rejects(prepareSingleSpriteFile(configFile, existing), { code: 'EEXIST' });
    assert.deepEqual(await readFile(existing), sentinel);
    await assert.rejects(prepareSingleSpriteFile(configFile, sourceFile), { code: 'EEXIST' });
    assert.deepEqual(await readFile(sourceFile), originalBytes);
  });
});

test('wrong encoded source hash is refused and creates no output', async () => {
  await withFileFixture(async ({ directory, config, configFile, sourceFile, originalBytes }) => {
    config.source.sha256 = hash(Buffer.from('not-the-review'));
    await writeFile(configFile, JSON.stringify(config));
    const output = path.join(directory, 'must-not-exist.png');
    await assert.rejects(prepareSingleSpriteFile(configFile, output), /hash differs/);
    await assert.rejects(readFile(output), { code: 'ENOENT' });
    assert.deepEqual(await readFile(sourceFile), originalBytes);
  });
});

test('wrong dimensions, nonsquare canvas, RGB and palette PNG inputs are refused', async () => {
  await withFileFixture(async ({ config, configFile, sourceFile }) => {
    for (const [width, height, channels, palette, message] of [
      [9, 9, 4, false, /dimensions/], [8, 7, 4, false, /dimensions/],
      [8, 8, 3, false, /true 8-bit RGBA/], [8, 8, 4, true, /true 8-bit RGBA/],
    ]) {
      const bytes = await sharp({ create: { width, height, channels, background: '#123456' } }).png({ palette }).toBuffer();
      await writeFile(sourceFile, bytes);
      config.source.sha256 = hash(bytes);
      await assert.rejects(loadReviewedSingleSource(config, configFile), message);
    }
  });
});

test('non-PNG and non-8-bit PNG IHDR are refused before decoder conversion', async () => {
  await withFileFixture(async ({ config, configFile, sourceFile, originalBytes }) => {
    const sixteenBitHeader = Buffer.from(originalBytes);
    sixteenBitHeader[24] = 16;
    for (const bytes of [Buffer.from('not-a-png'), sixteenBitHeader]) {
      await writeFile(sourceFile, bytes);
      config.source.sha256 = hash(bytes);
      await assert.rejects(loadReviewedSingleSource(config, configFile), /true 8-bit RGBA/);
    }
  });
});

test('visible source edge and invalid config never create an output or alter source', async () => {
  await withFileFixture(async ({ directory, config, configFile, sourceFile, source }) => {
    source.data[3] = 128;
    const bytes = await sharp(source.data, { raw: { width: 8, height: 8, channels: 4 } }).png().toBuffer();
    await writeFile(sourceFile, bytes);
    config.source.sha256 = hash(bytes);
    await writeFile(configFile, JSON.stringify(config));
    const output = path.join(directory, 'must-not-exist.png');
    await assert.rejects(prepareSingleSpriteFile(configFile, output), /touch a canvas edge/);
    assert.deepEqual(await readFile(sourceFile), bytes);
    await assert.rejects(readFile(output), { code: 'ENOENT' });
    config.outputFrame = 8;
    await writeFile(configFile, JSON.stringify(config));
    await assert.rejects(prepareSingleSpriteFile(configFile, output), /outputFrame/);
    await assert.rejects(readFile(output), { code: 'ENOENT' });
  });
});

test('CLI emits the single-frame source, output and selection manifest', async () => {
  await withFileFixture(async ({ directory, configFile, config, sourceFile, originalBytes }) => {
    const script = fileURLToPath(new URL('./prepare_reviewed_single_sprite.mjs', import.meta.url));
    const output = path.join(directory, 'single reviewed selection.png');
    const { stdout } = await execute(process.execPath, ['--jitless', script, configFile, output], { windowsHide: true });
    const report = JSON.parse(stdout);
    assert.equal(report.outputPath, output);
    assert.equal(report.outputFrame, 3);
    assert.equal(report.nonemptyFrameCount, 1);
    assert.equal(report.source.sha256, config.source.sha256);
    assert.equal(report.outputSha256, hash(await readFile(output)));
    assert.deepEqual(report.emptyFrames, [0, 1, 2, 4, 5, 6, 7]);
    assert.deepEqual(await readFile(sourceFile), originalBytes);
  });
});

test('invalid CLI arguments and non-PNG output extension are refused', async () => {
  const script = fileURLToPath(new URL('./prepare_reviewed_single_sprite.mjs', import.meta.url));
  await assert.rejects(execute(process.execPath, ['--jitless', script], { windowsHide: true }), /Usage:/);
  await withFileFixture(async ({ directory, configFile }) => {
    await assert.rejects(prepareSingleSpriteFile(configFile, path.join(directory, 'output.jpg')), /\.png/);
    await assert.rejects(readFile(path.join(directory, 'output.jpg')), { code: 'ENOENT' });
  });
});
