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
import { assembleFile, assembleReviewedRgba, loadReviewedSources, validateAssemblyConfig } from './assemble_reviewed_sprite_atlas.mjs';

const sharp = createRequire(import.meta.url)('sharp');
const hash = data => createHash('sha256').update(data).digest('hex');
const clone = value => JSON.parse(JSON.stringify(value));
const execute = promisify(execFile);

function syntheticSources(cellWidth = 3, cellHeight = 2) {
  const width = 4 * cellWidth, height = 2 * cellHeight;
  const sources = [0, 1].map(source => {
    const data = Buffer.alloc(width * height * 4);
    for (let frame = 0; frame < 8; frame++) for (let y = 0; y < cellHeight; y++) for (let x = 0; x < cellWidth; x++) {
      const offset = (((Math.floor(frame / 4) * cellHeight + y) * width) + frame % 4 * cellWidth + x) * 4;
      // Every selected frame is distinct. Keep nonzero transparent RGB and
      // partial alpha to detect unintended cleanup or implicit alteration.
      data.set([13 + source * 80 + frame, 37 + x, 91 + y, x === 0 && y === 0 ? 0 : (x === 1 ? 128 : 255)], offset);
    }
    return { path: `source-${source}.png`, data, width, height, channels: 4, sha256: hash(Buffer.from(`synthetic-reviewed-file-${source}`)) };
  });
  const config = {
    sources: sources.map(({ path, sha256 }) => ({ path, sha256 })),
    columns: 4, rows: 2, cellWidth, cellHeight,
    frameMap: Array.from({ length: 8 }, (_, frame) => ({ source: frame === 3 ? 1 : 0, frame })),
  };
  return { sources, config };
}

function cellBytes(source, frame, config) {
  const cell = Buffer.alloc(config.cellWidth * config.cellHeight * 4);
  for (let row = 0; row < config.cellHeight; row++) {
    const offset = ((Math.floor(frame / 4) * config.cellHeight + row) * source.width + frame % 4 * config.cellWidth) * 4;
    cell.set(source.data.subarray(offset, offset + config.cellWidth * 4), row * config.cellWidth * 4);
  }
  return cell;
}

test('seven reviewed frames plus one explicit repaired frame retain exact bytes and provenance', () => {
  const { sources, config } = syntheticSources();
  const before = sources.map(source => Buffer.from(source.data));
  const output = assembleReviewedRgba(config, sources);
  const decoded = { data: output.data, width: output.width };
  assert.equal(output.manifest.frameCount, 8);
  for (let frame = 0; frame < 8; frame++) {
    const pair = config.frameMap[frame];
    const expected = cellBytes(sources[pair.source], pair.frame, config);
    assert.deepEqual(cellBytes(decoded, frame, config), expected);
    assert.equal(output.manifest.frames[frame].source, pair.source);
    assert.equal(output.manifest.frames[frame].frame, pair.frame);
    assert.equal(output.manifest.frames[frame].sourceSha256, sources[pair.source].sha256);
    assert.equal(output.manifest.frames[frame].frameRgbaSha256, hash(expected));
  }
  assert.equal(output.manifest.outputRawRgbaSha256, hash(output.data));
  assert.match(output.manifest.acceptanceLimits, /No drawing/);
  sources.forEach((source, index) => assert.deepEqual(source.data, before[index]));
});

test('explicit reorder is honoured exactly; assembler never picks a better-looking frame', () => {
  const { sources, config } = syntheticSources();
  config.frameMap.reverse();
  const output = assembleReviewedRgba(config, sources);
  for (let frame = 0; frame < 8; frame++) {
    const pair = config.frameMap[frame];
    assert.deepEqual(cellBytes({ data: output.data, width: output.width }, frame, config), cellBytes(sources[pair.source], pair.frame, config));
  }
});

test('same explicit source/frame pair is rejected', () => {
  const { sources, config } = syntheticSources();
  config.frameMap[3] = { ...config.frameMap[0] };
  assert.throws(() => assembleReviewedRgba(config, sources), /Duplicate manual/);
});

test('same reviewed file hash and frame through a second source index is rejected', () => {
  const { sources, config } = syntheticSources();
  sources[1].sha256 = sources[0].sha256;
  config.sources[1].sha256 = sources[0].sha256;
  config.frameMap[3] = { source: 1, frame: 0 };
  assert.throws(() => assembleReviewedRgba(config, sources), /source alias/);
});

test('same decoded raw source and frame through differently encoded aliases is rejected', () => {
  const { sources, config } = syntheticSources();
  sources[1].data = Buffer.from(sources[0].data);
  config.frameMap[3] = { source: 1, frame: 0 };
  assert.throws(() => assembleReviewedRgba(config, sources), /source alias/);
});

test('different source/frame pairs cannot disguise exact duplicate selected pixels', () => {
  const { sources, config } = syntheticSources();
  const replacement = cellBytes(sources[0], 0, config);
  for (let row = 0; row < config.cellHeight; row++) {
    const offset = (row * sources[1].width + 3 * config.cellWidth) * 4;
    sources[1].data.set(replacement.subarray(row * config.cellWidth * 4, (row + 1) * config.cellWidth * 4), offset);
  }
  assert.throws(() => assembleReviewedRgba(config, sources), /exact RGBA bytes/);
});

test('fully transparent selected frame is rejected even when RGB is nonzero', () => {
  const { sources, config } = syntheticSources();
  for (let row = 0; row < config.cellHeight; row++) for (let x = 0; x < config.cellWidth; x++) {
    sources[1].data[(row * sources[1].width + 3 * config.cellWidth + x) * 4 + 3] = 0;
  }
  assert.throws(() => assembleReviewedRgba(config, sources), /empty transparent/);
});

test('source width/height/channel count and byte length are all validated', () => {
  for (const alteration of [source => source.width++, source => source.height++, source => source.channels = 3,
    source => source.data = source.data.subarray(1)]) {
    const { sources, config } = syntheticSources();
    alteration(sources[1]);
    assert.throws(() => assembleReviewedRgba(config, sources), /complete, same-size RGBA/);
  }
});

test('reviewed SHA mismatch is rejected before copying', () => {
  const { sources, config } = syntheticSources();
  sources[1].sha256 = hash(Buffer.from('wrong-file'));
  assert.throws(() => assembleReviewedRgba(config, sources), /file hash/);
});

test('invalid hashes, paths, grid dimensions, source count and map length are rejected', () => {
  const { config } = syntheticSources();
  for (const alteration of [
    item => item.sources[0].sha256 = 'abc', item => item.sources[0].path = '',
    item => item.sources[0].path = 'https://example.test/a.png', item => item.sources[0].path = 'a\0.png',
    item => item.columns = 2, item => item.rows = 4, item => item.cellWidth = 0,
    item => item.cellHeight = 1.5, item => item.cellWidth = 4097,
    item => { item.cellWidth = 4096; item.cellHeight = 4096; },
    item => item.sources = [], item => item.frameMap.pop(),
  ]) {
    const invalid = clone(config);
    alteration(invalid);
    assert.throws(() => validateAssemblyConfig(invalid));
  }
});

test('out-of-range and noninteger source/frame indices are rejected', () => {
  const { config } = syntheticSources();
  for (const [key, value] of [['source', -1], ['source', 2], ['source', 0.5], ['source', '0'],
    ['frame', -1], ['frame', 8], ['frame', 1.1], ['frame', '3']]) {
    const invalid = clone(config);
    invalid.frameMap[3][key] = value;
    assert.throws(() => validateAssemblyConfig(invalid), /out-of-range/);
  }
});

async function withFileFixture(callback, { cellWidth = 3, cellHeight = 2 } = {}) {
  const directory = await mkdtemp(path.join(os.tmpdir(), 'immortal-reviewed-assembly-'));
  try {
    const { sources, config } = syntheticSources(cellWidth, cellHeight);
    for (let index = 0; index < sources.length; index++) {
      const source = sources[index];
      const png = await sharp(source.data, { raw: { width: source.width, height: source.height, channels: 4 } }).png().toBuffer();
      config.sources[index].sha256 = hash(png);
      await writeFile(path.join(directory, source.path), png, { flag: 'wx' });
    }
    const configFile = path.join(directory, 'reviewed.json');
    await writeFile(configFile, JSON.stringify(config), { flag: 'wx' });
    await callback({ directory, config, configFile, sources });
  } finally {
    // Only this mkdtemp-created directory is removed, never a computed project/source path.
    await rm(directory, { recursive: true, force: false });
  }
}

test('real 512x512-cell PNG integration preserves every selected pixel and source file', async () => {
  await withFileFixture(async ({ directory, config, configFile, sources }) => {
    const inputBytes = await Promise.all(config.sources.map(source => readFile(path.join(directory, source.path))));
    const outputFile = path.join(directory, 'assembled.png');
    const report = await assembleFile(configFile, outputFile);
    const outputBytes = await readFile(outputFile);
    const { data, info } = await sharp(outputBytes).raw().toBuffer({ resolveWithObject: true });
    assert.deepEqual([info.width, info.height, info.channels], [2048, 1024, 4]);
    for (let frame = 0; frame < 8; frame++) {
      const pair = config.frameMap[frame];
      assert.deepEqual(cellBytes({ data, width: info.width }, frame, config), cellBytes(sources[pair.source], pair.frame, config));
    }
    assert.equal(report.outputSha256, hash(outputBytes));
    assert.equal(report.configSha256, hash(await readFile(configFile)));
    for (let index = 0; index < config.sources.length; index++) {
      assert.deepEqual(await readFile(path.join(directory, config.sources[index].path)), inputBytes[index]);
      assert.equal(report.sources[index].sha256, config.sources[index].sha256);
    }
  }, { cellWidth: 512, cellHeight: 512 });
});

test('exclusive output refuses an existing revision and leaves it unchanged', async () => {
  await withFileFixture(async ({ directory, configFile }) => {
    const outputFile = path.join(directory, 'existing.png');
    const sentinel = Buffer.from('existing-art-must-not-change');
    await writeFile(outputFile, sentinel, { flag: 'wx' });
    await assert.rejects(assembleFile(configFile, outputFile), { code: 'EEXIST' });
    assert.deepEqual(await readFile(outputFile), sentinel);
  });
});

test('single CLI emits the exact selection/source/output provenance manifest', async () => {
  await withFileFixture(async ({ directory, config, configFile }) => {
    const output = path.join(directory, 'manual choice with spaces.png');
    const script = fileURLToPath(new URL('./assemble_reviewed_sprite_atlas.mjs', import.meta.url));
    const { stdout } = await execute(process.execPath, ['--jitless', script, configFile, output], { windowsHide: true });
    const report = JSON.parse(stdout);
    assert.equal(report.outputPath, output);
    assert.equal(report.frames.length, 8);
    assert.deepEqual(report.frames.map(frame => ({ source: frame.source, frame: frame.frame })), config.frameMap);
    assert.equal(report.outputSha256, hash(await readFile(output)));
    assert.equal(report.sources[0].sha256, config.sources[0].sha256);
    assert.equal(report.sources[1].sha256, config.sources[1].sha256);
    assert.match(report.acceptanceLimits, /No drawing/);
  });
});

test('output cannot overwrite an original source', async () => {
  await withFileFixture(async ({ directory, config, configFile }) => {
    const original = path.join(directory, config.sources[0].path);
    const before = await readFile(original);
    await assert.rejects(assembleFile(configFile, original), { code: 'EEXIST' });
    assert.deepEqual(await readFile(original), before);
  });
});

test('changed PNG hash is refused by the file loader', async () => {
  await withFileFixture(async ({ config, configFile }) => {
    config.sources[0].sha256 = hash(Buffer.from('not-the-reviewed-PNG'));
    await assert.rejects(loadReviewedSources(config, configFile), /hash differs/);
  });
});

test('RGBA PNG with incorrect complete-grid dimensions is rejected', async () => {
  await withFileFixture(async ({ directory, config, configFile }) => {
    const source = path.join(directory, config.sources[1].path);
    const wrong = await sharp({ create: { width: 13, height: 4, channels: 4, background: '#123456ff' } }).png().toBuffer();
    await writeFile(source, wrong);
    config.sources[1].sha256 = hash(wrong);
    await assert.rejects(loadReviewedSources(config, configFile), /dimensions/);
  });
});

test('RGB PNG is rejected rather than silently adding alpha', async () => {
  await withFileFixture(async ({ directory, config, configFile }) => {
    const rgb = await sharp({ create: { width: 12, height: 4, channels: 3, background: '#123456' } }).png().toBuffer();
    await writeFile(path.join(directory, config.sources[0].path), rgb);
    config.sources[0].sha256 = hash(rgb);
    await assert.rejects(loadReviewedSources(config, configFile), /true 8-bit RGBA/);
  });
});

test('invalid config never creates an output file', async () => {
  await withFileFixture(async ({ directory, config, configFile }) => {
    config.frameMap[3].frame = 99;
    await writeFile(configFile, JSON.stringify(config));
    const output = path.join(directory, 'must-not-exist.png');
    await assert.rejects(assembleFile(configFile, output), /out-of-range/);
    await assert.rejects(readFile(output), { code: 'ENOENT' });
  });
});
