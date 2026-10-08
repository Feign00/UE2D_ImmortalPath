#!/usr/bin/env node
// Read-only reconstruction of the explicitly reviewed Qingyun dog/wolf artwork.
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
const projectRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const root = path.join(projectRoot, 'ArtSource/MortalRealm/DesktopPixelV2/Dog');
const sha = bytes => createHash('sha256').update(bytes).digest('hex');
const actions = ['idle', 'move', 'attack', 'hurt', 'death'];
const rates = [8, 8, 12, 12, 10];
const results = [];
for (const action of actions) {
  const regions = JSON.parse(await readFile(path.join(root, `${action}-regions.json`), 'utf8'));
  assert.equal(regions.source, `sources/${action}-original-v1.png`);
  assert.equal(regions.alphaThreshold, 128);
  assert.equal(regions.columns, 4);
  assert.equal(regions.rows, 2);
  assert.deepEqual(regions.outputFrameSize, [512, 512]);
  assert.equal(regions.componentSeeds.length, 8, `${action}: all frame ownership explicitly authored`);
  const original = await readFile(path.resolve(root, regions.source));
  validateSourceSha256(original, regions.sourceSha256);
  const { data, info } = await sharp(original).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
  assert.deepEqual([info.width, info.height], [1536, 1024]);
  let visibleSourcePixels = 0;
  for (let i = 3; i < data.length; i += 4) if (data[i] >= 128) visibleSourcePixels++;
  const regrid = prepareAuthoredRegions(data, info.width, info.height, regions);
  assert.equal(regrid.regionMode, 'authored-four-neighbour-components');
  assert.equal(regrid.visiblePixelCount, visibleSourcePixels, `${action}: every threshold-visible original pixel retained once`);
  assert.equal(regrid.frames.reduce((sum, frame) => sum + frame.pixelCount, 0), visibleSourcePixels);
  assert.equal(regrid.frames.reduce((sum, frame) => sum + frame.components.length, 0), regrid.componentCount,
    `${action}: all connected components, including isolated pixels, explicitly assigned once`);
  const rebuiltRegrid = await sharp(regrid.data, {
    raw: { width: regrid.width, height: regrid.height, channels: 4 }
  }).png().toBuffer();
  const expectedRegrid = await readFile(path.join(root, 'sources', `${action}-regrid-v1.png`));
  assert.deepEqual(rebuiltRegrid, expectedRegrid, `${action}: extraction is byte-reproducible`);
  const alignment = JSON.parse(await readFile(path.join(root, `${action}-alignment.json`), 'utf8'));
  assert.equal(alignment.source, `sources/${action}-regrid-v1.png`);
  assert.equal(sha(expectedRegrid), alignment.sourceSha256, `${action}: reviewed alignment input`);
  assert.equal(alignment.scale, 0.7, `${action}: shared whole-clip scale, never per-frame`);
  assert.equal(alignment.columns, 4);
  assert.equal(alignment.rows, 2);
  assert.equal(alignment.alphaThreshold, 128);
  assert.equal(alignment.bottomTolerance, action === 'death' ? 4 : 2);
  assert.deepEqual(alignment.targetAnchor, [256, 336]);
  assert.deepEqual(alignment.outputFrameSize, [512, 384]);
  assert.ok(typeof alignment.anchorReview === 'string' && alignment.anchorReview.length > 40);
  const rgba = alignRgba(regrid.data, regrid.width, regrid.height, alignment);
  const atlas = await sharp(rgba, { raw: { width: 2048, height: 768, channels: 4 } }).png().toBuffer();
  assert.deepEqual(atlas, await readFile(path.join(root, `${action}-v1`, 'atlas.png')),
    `${action}: final atlas is byte-reproducible from unchanged original`);
  const report = auditRgba(rgba, 2048, 768, 4, 2, { bottomTolerance: action === 'death' ? 4 : 2 });
  assert.deepEqual(report.warnings, [], `${action}: no transparency/crop/repeated-frame/drift warnings`);
  assert.equal(new Set(report.frames.map(frame => frame.maskedSha256)).size, 8);
  assert.equal(report.frames.reduce((sum, frame) => sum + frame.partial, 0), 0, `${action}: hard alpha only`);
  assert.ok(report.frames.every(frame => frame.clear > 0 && frame.opaque > 0 && !frame.touchesCellEdge));
  let magenta = 0;
  for (let i = 0; i < rgba.length; i += 4) {
    if (rgba[i + 3] === 255 && rgba[i] > 200 && rgba[i + 2] > 200 && rgba[i + 1] < 80) magenta++;
  }
  assert.equal(magenta, 0, `${action}: no opaque magenta key pixels`);
  const recorded = JSON.parse(await readFile(path.join(root, `${action}-v1`, 'review.json'), 'utf8'));
  assert.equal(recorded.sourceSha256, alignment.sourceSha256);
  assert.deepEqual(recorded.settings, alignment, `${action}: saved review contains current manual settings`);
  assert.deepEqual(recorded.report, report, `${action}: saved audit matches rebuilt pixels`);
  results.push({ action, sourceSha256: sha(original), atlasSha256: sha(atlas),
    frames: 8, hardAlpha: true, magentaPixels: magenta,
    silhouetteBottomSpread: report.silhouetteBottomSpread,
    retainedSourcePixels: regrid.visiblePixelCount,
    explicitlyAssignedComponents: regrid.componentCount });
}

const importSettings = JSON.parse(await readFile(path.join(projectRoot, 'Config/ImportDesktopPixelDog.json'), 'utf8'));
assert.equal(importSettings.ImportGroups.length, 5);
assert.equal(importSettings.FlipbookGroups.length, 5);
for (const [index, action] of actions.entries()) {
  const name = action[0].toUpperCase() + action.slice(1);
  const group = importSettings.ImportGroups[index];
  assert.equal(group.GroupName, `DesktopPixelDog${name}`);
  assert.equal(group.bEnabled, true);
  assert.deepEqual(group.Filenames, [`ArtSource/MortalRealm/DesktopPixelV2/Dog/${action}-v1/atlas.png`]);
  assert.equal(group.DestinationPath, `/Game/GAME/Asset/Monsters/desktop_pixel_v2/dog/${action}`);
  assert.equal(group.DestinationName, `T_Dog_Pixel_${name}`);
  assert.equal(group.SpriteDestinationPath, `${group.DestinationPath}/sprites`);
  assert.equal(group.SpriteName, `SP_Dog_Pixel_${name}`);
  assert.equal(group.FactoryName, 'TextureFactory');
  assert.equal(group.TextureUsage, 'Sprite');
  assert.equal(group.ExpectedFrameCount, 8);
  assert.equal(group.ExpectedWidth, 2048);
  assert.equal(group.ExpectedHeight, 768);
  assert.equal(group.bCreateSprites, true);
  assert.equal(group.bSpriteSheet, true);
  assert.equal(group.SheetColumns, 4);
  assert.equal(group.SheetRows, 2);
  assert.equal(group.SheetFrameCount, 8);
  assert.equal(group.PixelsPerUnrealUnit, 2.56);
  assert.deepEqual(group.SourceUV, [0, 0]);
  assert.deepEqual(group.SourceDimension, [512, 384]);
  assert.equal(group.PivotMode, 'Custom');
  assert.deepEqual(group.CustomPivot, [256, 336]);
  assert.equal(group.MaterialMode, 'Masked');
  const flipbook = importSettings.FlipbookGroups[index];
  assert.equal(flipbook.GroupName, `DesktopPixelDog${name}Flipbook`);
  assert.equal(flipbook.bEnabled, true);
  assert.equal(flipbook.SourceImportGroup, group.GroupName);
  assert.equal(flipbook.DestinationPath, '/Game/GAME/Asset/Monsters/desktop_pixel_v2/dog');
  assert.equal(flipbook.AssetName, `FB_Dog_Pixel_${name}`);
  assert.equal(flipbook.FramesPerSecond, rates[index]);
  assert.equal(flipbook.FrameRun, 1);
}
console.log(JSON.stringify({ result: 'PASS', actions: 5, authoredFrames: 40, results,
  note: 'Original threshold-visible pixels are preserved once in the translation-only regrid. Final uniform downsampling and static QA are not runtime or subjective motion acceptance.' }, null, 2));
