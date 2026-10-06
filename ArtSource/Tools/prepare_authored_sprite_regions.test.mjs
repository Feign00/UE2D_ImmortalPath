import test from 'node:test';
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { mkdtemp, readFile, writeFile, unlink, rmdir } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { prepareAuthoredRegions, validateSourceSha256, writeExclusiveOutput } from './prepare_authored_sprite_regions.mjs';

function fixture() {
  const width = 20, height = 12;
  const sourceRects = [[0, 0, 4, 4], [4, 0, 5, 5], [9, 0, 6, 4], [15, 0, 4, 6],
    [0, 6, 5, 5], [5, 6, 4, 6], [9, 6, 6, 6], [15, 6, 4, 6]];
  const data = Buffer.alloc(width * height * 4);
  sourceRects.forEach(([x, y], frame) => data.set([21 + frame, 61 + frame, 101 + frame, 128 + frame],
    ((y + 1) * width + x + 1) * 4));
  data.set([201, 202, 203, 127], (11 * width + 19) * 4);
  return { data, width, height, settings: { sourceRects, outputFrameSize: [7, 7] } };
}

test('non-uniform reviewed rectangles are translated with original RGB and hard alpha, without altering inputs', () => {
  const { data, width, height, settings } = fixture();
  const before = Buffer.from(data), settingsBefore = structuredClone(settings);
  const result = prepareAuthoredRegions(data, width, height, settings);
  assert.equal(result.width, 28);
  assert.equal(result.height, 14);
  assert.equal(result.visiblePixelCount, 8);
  for (let frame = 0; frame < 8; frame++) {
    const dest = ((Math.floor(frame / 4) * 7 + 1) * result.width + frame % 4 * 7 + 1) * 4;
    assert.deepEqual([...result.data.subarray(dest, dest + 4)], [21 + frame, 61 + frame, 101 + frame, 255]);
    assert.deepEqual(result.frames[frame].bounds, { left: 1, top: 1, right: 1, bottom: 1 });
    assert.equal(result.frames[frame].pixelCount, 1);
    const [x, y] = settings.sourceRects[frame];
    assert.deepEqual(result.frames[frame].sourceBounds, { left: x + 1, top: y + 1, right: x + 1, bottom: y + 1 });
  }
  assert.equal([...result.data].filter((value, index) => index % 4 === 3 && value !== 0).length, 8);
  assert.ok([...result.data].every((value, index) => index % 4 !== 3 || value === 0 || value === 255));
  assert.deepEqual(data, before);
  assert.deepEqual(settings, settingsBefore);
  assert.deepEqual(result.data, prepareAuthoredRegions(data, width, height, settings).data);
});

test('default layout is four by two 512px cells and transparent-only region overlap is allowed', () => {
  const { data, width, height, settings } = fixture();
  settings.sourceRects[0][2] = 5;
  delete settings.outputFrameSize;
  const result = prepareAuthoredRegions(data, width, height, settings);
  assert.equal(result.width, 2048);
  assert.equal(result.height, 1024);
  assert.equal(result.columns, 4);
  assert.equal(result.rows, 2);
  assert.equal(result.alphaThreshold, 128);
  assert.equal(result.visiblePixelCount, 8);
});

test('missing or multiply-owned visible source pixels are rejected rather than discarded or duplicated', () => {
  const missing = fixture();
  missing.data.set([2, 4, 6, 200], (1 * missing.width + 19) * 4);
  assert.throws(() => prepareAuthoredRegions(missing.data, missing.width, missing.height, missing.settings), /missing/);
  const overlap = fixture();
  overlap.settings.sourceRects[0][2] = 7;
  assert.throws(() => prepareAuthoredRegions(overlap.data, overlap.width, overlap.height, overlap.settings), /overlaps/);
});

test('visible contact with any authored rectangle edge is rejected, including a tail tip', () => {
  for (const [x, y] of [[0, 1], [1, 0], [3, 1], [1, 3]]) {
    const { data, width, height, settings } = fixture();
    data.set([88, 77, 66, 255], (y * width + x) * 4);
    assert.throws(() => prepareAuthoredRegions(data, width, height, settings), /boundary/);
  }
});

test('rejects empty frames, malformed grids, unsafe dimensions, rectangles, frame counts and thresholds', () => {
  const f = fixture();
  const invalid = [
    null, { sourceRects: [] }, { sourceRects: f.settings.sourceRects.slice(0, 7) },
    { ...f.settings, columns: 0 }, { ...f.settings, columns: null }, { ...f.settings, columns: 3, rows: 3 },
    { ...f.settings, rows: 1 }, { ...f.settings, columns: 4.5 },
    { ...f.settings, outputFrameSize: [2, 7] }, { ...f.settings, outputFrameSize: [7, 7.5] },
    { ...f.settings, outputFrameSize: [4096, 4096] }, { ...f.settings, outputFrameSize: [7] },
    { ...f.settings, outputFrameSize: null },
    { ...f.settings, alphaThreshold: 0 }, { ...f.settings, alphaThreshold: 256 },
    { ...f.settings, alphaThreshold: '128' }
  ];
  for (const settings of invalid) assert.throws(() => prepareAuthoredRegions(f.data, f.width, f.height, settings));
  for (const rect of [[-1, 0, 4, 4], [0, 0, 0, 4], [0, 0, 4.5, 4], [19, 0, 4, 4], [0, 11, 4, 4], [0, 0, 4]]) {
    const settings = structuredClone(f.settings);
    settings.sourceRects[0] = rect;
    assert.throws(() => prepareAuthoredRegions(f.data, f.width, f.height, settings));
  }
  assert.throws(() => prepareAuthoredRegions(f.data.subarray(4), f.width, f.height, f.settings));
  assert.throws(() => prepareAuthoredRegions(f.data, 0, f.height, f.settings));
  const empty = fixture();
  empty.data.fill(0, (1 * empty.width + 1) * 4, (1 * empty.width + 1) * 4 + 4);
  assert.throws(() => prepareAuthoredRegions(empty.data, empty.width, empty.height, empty.settings), /no visible/);
});

test('SHA256 mismatch and malformed hashes are rejected; matching hash is case-insensitive', () => {
  const bytes = Buffer.from('reviewed original image');
  const hash = createHash('sha256').update(bytes).digest('hex');
  assert.equal(validateSourceSha256(bytes, hash), hash);
  assert.equal(validateSourceSha256(bytes, hash.toUpperCase()), hash);
  assert.throws(() => validateSourceSha256(bytes, '0'.repeat(64)), /differs/);
  for (const value of ['', '123', 'z'.repeat(64), null]) assert.throws(() => validateSourceSha256(bytes, value));
});

function seededFixture() {
  const f = fixture();
  f.settings.componentSeeds = f.settings.sourceRects.map(([x, y]) => [[x + 1, y + 1]]);
  return f;
}

test('authored component seeds separate contours whose bounding rectangles overlap without cutting tails', () => {
  const { data, width, height, settings } = seededFixture();
  settings.sourceRects[0] = [0, 0, 7, 6];
  // A connected hooked tail wraps below frame 1's disconnected pixel. Their
  // bounding boxes overlap, but no silhouette pixel is shared or redrawn.
  for (const [x, y] of [[1, 2], [1, 3], [1, 4], [2, 4], [3, 4], [4, 4], [5, 4]])
    data.set([21, 61, 101, 255], (y * width + x) * 4);
  const before = Buffer.from(data), settingsBefore = structuredClone(settings);
  const result = prepareAuthoredRegions(data, width, height, settings);
  assert.equal(result.regionMode, 'authored-four-neighbour-components');
  assert.equal(result.componentCount, 8);
  assert.equal(result.visiblePixelCount, 15);
  assert.equal(result.frames[0].pixelCount, 8);
  assert.equal(result.frames[1].pixelCount, 1);
  assert.deepEqual(result.frames[0].bounds, { left: 1, top: 1, right: 5, bottom: 4 });
  assert.deepEqual([...result.data.subarray((4 * result.width + 5) * 4, (4 * result.width + 5) * 4 + 4)], [21, 61, 101, 255]);
  const second = (1 * result.width + 7 + 1) * 4;
  assert.deepEqual([...result.data.subarray(second, second + 4)], [22, 62, 102, 255]);
  assert.deepEqual(data, before);
  assert.deepEqual(settings, settingsBefore);
  assert.deepEqual(result.data, prepareAuthoredRegions(data, width, height, settings).data);
  const rectOnly = { ...settings };
  delete rectOnly.componentSeeds;
  assert.throws(() => prepareAuthoredRegions(data, width, height, rectOnly), /overlaps/);
});

test('every visible component including isolated pixels needs an explicit frame seed', () => {
  const { data, width, height, settings } = seededFixture();
  data.set([202, 17, 93, 128], (9 * width + 17) * 4);
  assert.throws(() => prepareAuthoredRegions(data, width, height, settings), /no authored seed/);
  settings.componentSeeds[7].push([17, 9]);
  const result = prepareAuthoredRegions(data, width, height, settings);
  assert.equal(result.componentCount, 9);
  assert.equal(result.visiblePixelCount, 9);
  assert.equal(result.frames[7].components.length, 2);
  const dest = ((7 + 9 - 6) * result.width + 3 * 7 + 17 - 15) * 4;
  assert.deepEqual([...result.data.subarray(dest, dest + 4)], [202, 17, 93, 255]);
});

test('connectivity is explicitly four-neighbour; diagonal pixels need separate manual seeds', () => {
  const { data, width, height, settings } = seededFixture();
  data.set([67, 89, 123, 255], (2 * width + 2) * 4);
  assert.throws(() => prepareAuthoredRegions(data, width, height, settings), /no authored seed/);
  settings.componentSeeds[0].push([2, 2]);
  const result = prepareAuthoredRegions(data, width, height, settings);
  assert.equal(result.frames[0].components.length, 2);
  assert.equal(result.frames[0].pixelCount, 2);
});

test('component seeds must be valid visible source pixels and have eight non-empty groups', () => {
  for (const seed of [[0, 0], [-1, 1], [20, 1], [1, 12], [1.5, 1], [1], 'bad']) {
    const { data, width, height, settings } = seededFixture();
    settings.componentSeeds[0] = [seed];
    assert.throws(() => prepareAuthoredRegions(data, width, height, settings), /seed/);
  }
  for (const seeds of [null, [], Array.from({ length: 7 }, () => [[1, 1]]), Array.from({ length: 8 }, () => [])]) {
    const { data, width, height, settings } = seededFixture();
    settings.componentSeeds = seeds;
    assert.throws(() => prepareAuthoredRegions(data, width, height, settings), /componentSeeds/);
  }
});

test('one connected component cannot be selected twice within or across frames', () => {
  const sameFrame = seededFixture();
  sameFrame.settings.componentSeeds[0].push([1, 1]);
  assert.throws(() => prepareAuthoredRegions(sameFrame.data, sameFrame.width, sameFrame.height, sameFrame.settings), /more than once/);
  const crossFrame = seededFixture();
  crossFrame.settings.componentSeeds[1].push([1, 1]);
  assert.throws(() => prepareAuthoredRegions(crossFrame.data, crossFrame.width, crossFrame.height, crossFrame.settings), /multiple frames/);
});

test('selected components must fit completely inside their frame rectangle with clear boundary margin', () => {
  const outside = seededFixture();
  // Continue frame 0's connected tail past its rectangle while remaining on
  // canvas. A seed cannot authorize cropping part of the selected component.
  for (let x = 2; x <= 4; x++) outside.data.set([21, 61, 101, 255], (1 * outside.width + x) * 4);
  assert.throws(() => prepareAuthoredRegions(outside.data, outside.width, outside.height, outside.settings), /completely inside/);
  const edge = seededFixture();
  edge.settings.sourceRects[0] = [1, 0, 3, 4];
  assert.throws(() => prepareAuthoredRegions(edge.data, edge.width, edge.height, edge.settings), /boundary/);
});

test('exclusive output refuses to overwrite an existing source or earlier output', async () => {
  const directory = await mkdtemp(path.join(tmpdir(), 'immortal-authored-regions-'));
  const output = path.join(directory, 'atlas.png');
  try {
    await writeFile(output, Buffer.from('original source remains unchanged'));
    await assert.rejects(writeExclusiveOutput(output, Buffer.from('replacement')), { code: 'EEXIST' });
    assert.equal((await readFile(output)).toString(), 'original source remains unchanged');
    await unlink(output);
    await writeExclusiveOutput(output, Buffer.from('new output'));
    assert.equal((await readFile(output)).toString(), 'new output');
  } finally {
    await unlink(output);
    await rmdir(directory);
  }
});
