import test from 'node:test';
import assert from 'node:assert/strict';
import { auditRgba } from './audit_sprite_atlas.mjs';

test('rejects partial grid and malformed input', () => {
  assert.throws(() => auditRgba(Buffer.alloc(36), 3, 3, 2, 1));
  assert.throws(() => auditRgba(Buffer.alloc(1), 1, 1, 1, 1));
});
test('reports genuine transparency, partial alpha and masked bounds without modifying source', () => {
  const rgba = Buffer.alloc(4 * 4 * 4);
  rgba[(1 * 4 + 2) * 4 + 3] = 255;
  rgba[(2 * 4 + 1) * 4 + 3] = 127;
  const before = Buffer.from(rgba);
  const result = auditRgba(rgba, 4, 4, 1, 1);
  assert.equal(result.frames[0].clear, 14);
  assert.equal(result.frames[0].partial, 1);
  assert.equal(result.frames[0].solid, 1);
  assert.deepEqual(result.frames[0].bounds, { left: 2, top: 1, right: 2, bottom: 1 });
  assert.equal(result.frames[0].touchesCellEdge, false);
  assert.deepEqual(rgba, before);
});
test('detects opaque backgrounds, repeated frames and edge clipping', () => {
  const rgba = Buffer.alloc(4 * 2 * 4, 255);
  const result = auditRgba(rgba, 4, 2, 2, 1);
  assert.equal(result.warnings.length, 3);
  assert.equal(result.frames[0].touchesCellEdge, true);
  assert.equal(result.silhouetteBottomSpread, 0);
});

test('detects foot-line drift and lack of opaque interiors in generated atlases', () => {
  const rgba = Buffer.alloc(8 * 6 * 4);
  rgba[(2 * 8 + 1) * 4 + 3] = 253;
  rgba[(4 * 8 + 5) * 4 + 3] = 253;
  const result = auditRgba(rgba, 8, 6, 2, 1, { bottomTolerance: 1 });
  assert.equal(result.silhouetteBottomSpread, 2);
  assert.equal(result.frames[0].maxAlpha, 253);
  assert.ok(result.warnings.some(w => w.includes('bottom drift')));
  assert.ok(result.warnings.some(w => w.includes('no fully opaque')));
});

test('masked hash ignores hidden RGB and visible alpha differences', () => {
  const rgba = Buffer.alloc(6 * 3 * 4);
  rgba[0] = 200; // Invisible background difference is not a new drawn frame.
  rgba[(1 * 6 + 1) * 4 + 3] = 200;
  rgba[(1 * 6 + 4) * 4 + 3] = 255;
  const result = auditRgba(rgba, 6, 3, 2, 1);
  assert.notEqual(result.frames[0].sha256, result.frames[1].sha256);
  assert.equal(result.frames[0].maskedSha256, result.frames[1].maskedSha256);
  assert.ok(result.warnings.some(w => w.includes('Masked-identical')));
});

test('threshold is explicit and invalid review limits are rejected', () => {
  const rgba = Buffer.alloc(3 * 3 * 4);
  rgba[(1 * 3 + 1) * 4 + 3] = 100;
  assert.equal(auditRgba(rgba, 3, 3, 1, 1, { alphaThreshold: 85 }).frames[0].solid, 1);
  assert.equal(auditRgba(rgba, 3, 3, 1, 1).frames[0].solid, 0);
  for (const alphaThreshold of [0, 256, 1.5, NaN]) {
    assert.throws(() => auditRgba(rgba, 3, 3, 1, 1, { alphaThreshold }));
  }
  assert.throws(() => auditRgba(rgba, 3, 3, 1, 1, { bottomTolerance: -1 }));
});
