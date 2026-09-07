import test from 'node:test';
import assert from 'node:assert/strict';
import { alignRgba } from './align_sprite_atlas.mjs';

const settings = { columns: 2, rows: 1, anchors: [[1, 2], [2, 3]], targetAnchor: [2, 2] };
function fixture() {
  const data = Buffer.alloc(8 * 4 * 4);
  data.set([90, 160, 200, 253], (2 * 8 + 1) * 4);
  data.set([70, 150, 210, 253], (3 * 8 + 6) * 4);
  data.set([20, 80, 60, 127], 0);
  return data;
}
test('manual anchors align to common foot point with hard alpha and source preserved', () => {
  const input = fixture(), before = Buffer.from(input);
  const result = alignRgba(input, 8, 4, settings);
  assert.deepEqual(input, before);
  assert.deepEqual([...result.subarray((2 * 8 + 2) * 4, (2 * 8 + 2) * 4 + 4)], [90, 160, 200, 255]);
  assert.deepEqual([...result.subarray((2 * 8 + 6) * 4, (2 * 8 + 6) * 4 + 4)], [70, 150, 210, 255]);
  assert.equal(result[3], 0);
  assert.deepEqual(result, alignRgba(input, 8, 4, settings));
});
test('rejects bad manual anchors, scales and visible clipping', () => {
  for (const changes of [{ anchors: [] }, { anchors: [[1, 2], [4, 3]] }, { scale: 0 }, { scale: [1, 2] }, { targetAnchor: [4, 2] }])
    assert.throws(() => alignRgba(fixture(), 8, 4, { ...settings, ...changes }));
  const input = fixture(); input.set([255, 255, 255, 255], (1 * 8 + 3) * 4);
  assert.throws(() => alignRgba(input, 8, 4, settings), /clip/);
});

test('larger output canvas and explicit pose ordering do not duplicate frames', () => {
  const result = alignRgba(fixture(), 8, 4, { ...settings, outputFrameSize: [6, 6], frameOrder: [1, 0] });
  assert.equal(result.length, 12 * 6 * 4);
  assert.deepEqual([...result.subarray((2 * 12 + 2) * 4, (2 * 12 + 2) * 4 + 4)], [70, 150, 210, 255]);
  assert.deepEqual([...result.subarray((2 * 12 + 8) * 4, (2 * 12 + 8) * 4 + 4)], [90, 160, 200, 255]);
  for (const frameOrder of [[0, 0], [1], [-1, 0], [0, 2]])
    assert.throws(() => alignRgba(fixture(), 8, 4, { ...settings, frameOrder }));
});
