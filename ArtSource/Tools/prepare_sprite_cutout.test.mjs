import test from 'node:test';
import assert from 'node:assert/strict';
import { prepareCutout } from './prepare_sprite_cutout.mjs';

test('keeps original RGB, hard alpha and transparent padding without modifying inputs', () => {
  const original = Buffer.from([10, 20, 30, 255, 40, 50, 60, 255]);
  const mask = Buffer.from([255, 0, 0, 129, 255, 255, 0, 127]);
  const before = Buffer.from(original), maskBefore = Buffer.from(mask);
  const result = prepareCutout(original, mask, 2, 1, { paddingRight: 1, paddingBottom: 1 });
  assert.equal(result.width, 3); assert.equal(result.height, 2);
  assert.deepEqual([...result.data.subarray(0, 4)], [10, 20, 30, 255]);
  assert.ok(result.data.subarray(4).every(n => n === 0));
  assert.deepEqual(original, before); assert.deepEqual(mask, maskBefore);
});

test('removes connected neutral backdrop but preserves enclosed ivory and black outlines', () => {
  const original = Buffer.alloc(5 * 5 * 4, 255), mask = Buffer.from(original);
  for (let y = 1; y < 4; y++) for (let x = 1; x < 4; x++) {
    if (x !== 2 || y !== 2) original.set([10, 12, 10, 255], (y * 5 + x) * 4);
  }
  const result = prepareCutout(original, mask, 5, 5, { removeBrightBackdrop: true });
  assert.equal(result.data[3], 0);
  assert.equal(result.data[(2 * 5 + 2) * 4 + 3], 255);
  assert.deepEqual([...result.data.subarray(24, 28)], [10, 12, 10, 255]);
  assert.throws(() => prepareCutout(original, mask.subarray(4), 5, 5));
  assert.throws(() => prepareCutout(original, mask, 5, 5, { paddingRight: -1 }));
});
