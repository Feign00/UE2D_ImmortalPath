#!/usr/bin/env node
// Read-only end-to-end reconstruction of the authored Qingyun fox assets.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { createRequire } from 'node:module';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { prepareAuthoredRegions, validateSourceSha256 } from './prepare_authored_sprite_regions.mjs';
import { alignRgba } from './align_sprite_atlas.mjs';
import { auditRgba } from './audit_sprite_atlas.mjs';

const sharp = createRequire(import.meta.url)('sharp');
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../MortalRealm/DesktopPixelV2/Fox');
const sha = bytes => createHash('sha256').update(bytes).digest('hex');
const results = [];
for (const action of ['idle', 'move', 'attack', 'hurt', 'death']) {
  const regions = JSON.parse(await readFile(path.join(root, `${action}-regions.json`), 'utf8'));
  const original = await readFile(path.resolve(root, regions.source));
  validateSourceSha256(original, regions.sourceSha256);
  const { data, info } = await sharp(original).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
  const regrid = prepareAuthoredRegions(data, info.width, info.height, regions);
  const rebuiltRegrid = await sharp(regrid.data, {
    raw: { width: regrid.width, height: regrid.height, channels: 4 }
  }).png().toBuffer();
  const expectedRegrid = await readFile(path.join(root, 'sources', `${action}-regrid-v1.png`));
  assert.deepEqual(rebuiltRegrid, expectedRegrid, `${action}: region extraction is byte-reproducible`);
  const alignment = JSON.parse(await readFile(path.join(root, `${action}-alignment.json`), 'utf8'));
  assert.equal(sha(expectedRegrid), alignment.sourceSha256, `${action}: reviewed alignment input`);
  assert.equal(alignment.scale, 0.7, `${action}: one shared whole-clip scale, never per-frame`);
  assert.deepEqual(alignment.targetAnchor, [256, 336]);
  assert.deepEqual(alignment.outputFrameSize, [512, 384]);
  const rgba = alignRgba(regrid.data, regrid.width, regrid.height, alignment);
  const atlas = await sharp(rgba, { raw: { width: 2048, height: 768, channels: 4 } }).png().toBuffer();
  assert.deepEqual(atlas, await readFile(path.join(root, `${action}-v1`, 'atlas.png')),
    `${action}: final atlas is byte-reproducible from unchanged source`);
  const report = auditRgba(rgba, 2048, 768, 4, 2, { bottomTolerance: action === 'death' ? 4 : 2 });
  assert.deepEqual(report.warnings, [], `${action}: no transparency/crop/repeated-frame/drift warnings`);
  assert.equal(new Set(report.frames.map(f => f.maskedSha256)).size, 8);
  assert.equal(report.frames.reduce((sum, f) => sum + f.partial, 0), 0, `${action}: hard alpha only`);
  let magenta = 0;
  for (let i = 0; i < rgba.length; i += 4) {
    if (rgba[i + 3] === 255 && rgba[i] > 200 && rgba[i + 2] > 200 && rgba[i + 1] < 80) magenta++;
  }
  assert.equal(magenta, 0, `${action}: no opaque magenta key pixels`);
  const recorded = JSON.parse(await readFile(path.join(root, `${action}-v1`, 'review.json'), 'utf8'));
  assert.deepEqual(recorded.settings, alignment, `${action}: saved review has the current manual settings`);
  assert.deepEqual(recorded.report, report, `${action}: saved audit matches reconstructed pixels`);
  results.push({ action, sourceSha256: sha(original), atlasSha256: sha(atlas),
    frames: 8, hardAlpha: true, magentaPixels: magenta,
    silhouetteBottomSpread: report.silhouetteBottomSpread,
    retainedSourcePixels: regrid.visiblePixelCount });
}
console.log(JSON.stringify({ result: 'PASS', actions: 5, authoredFrames: 40, results,
  note: 'Pixel reconstruction and static QA are not runtime or subjective motion acceptance.' }, null, 2));
