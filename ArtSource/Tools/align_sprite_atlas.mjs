// Deterministic, explicitly authored anchor alignment. Never infers a per-frame scale.
import { createRequire } from 'node:module';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { pathToFileURL } from 'node:url';
import path from 'node:path';
import { auditRgba } from './audit_sprite_atlas.mjs';

export function alignRgba(data, width, height, settings) {
  const { columns, rows, anchors, targetAnchor, scale = 1, alphaThreshold = 128 } = settings;
  const original = auditRgba(data, width, height, columns, rows, { alphaThreshold });
  const { cellWidth: cw, cellHeight: ch } = original;
  const pointInCell = p => Array.isArray(p) && p.length === 2
    && p.every(Number.isInteger) && p[0] >= 0 && p[0] < cw && p[1] >= 0 && p[1] < ch;
  if (!Array.isArray(anchors) || anchors.length !== columns * rows || !anchors.every(pointInCell)
      || !pointInCell(targetAnchor) || !Number.isFinite(scale) || scale <= 0 || scale > 4) {
    throw new Error('Supply one valid manual anchor per frame and a single scale for the whole clip.');
  }
  const output = Buffer.alloc(data.length);
  for (let frame = 0; frame < anchors.length; frame++) {
    const [ax, ay] = anchors[frame], [tx, ty] = targetAnchor;
    const originX = frame % columns * cw, originY = Math.floor(frame / columns) * ch;
    const offset = (x, y) => ((originY + y) * width + originX + x) * 4;
    // Reject clipping instead of silently losing a sword, ponytail or planted foot.
    for (let y = 0; y < ch; y++) for (let x = 0; x < cw; x++) {
      if (data[offset(x, y) + 3] < alphaThreshold) continue;
      const dx = Math.round((x - ax) * scale + tx), dy = Math.round((y - ay) * scale + ty);
      if (dx < 0 || dx >= cw || dy < 0 || dy >= ch) throw new Error(`Frame ${frame} would clip visible pixels.`);
    }
    // Inverse nearest-neighbour sampling preserves aspect ratio and crisp pixel edges.
    for (let y = 0; y < ch; y++) for (let x = 0; x < cw; x++) {
      const sx = Math.round((x - tx) / scale + ax), sy = Math.round((y - ty) / scale + ay);
      if (sx < 0 || sx >= cw || sy < 0 || sy >= ch) continue;
      const source = offset(sx, sy), dest = offset(x, y);
      if (data[source + 3] < alphaThreshold) continue;
      output.set(data.subarray(source, source + 3), dest);
      output[dest + 3] = 255;
    }
  }
  return output;
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  const [configFile, outputDirectory] = process.argv.slice(2);
  if (!configFile || !outputDirectory || process.argv.length !== 4) throw new Error('Usage: align_sprite_atlas.mjs config.json NEW-output-directory');
  const configPath = path.resolve(configFile), outDir = path.resolve(outputDirectory);
  const config = JSON.parse(await readFile(configPath, 'utf8'));
  const sourcePath = path.resolve(path.dirname(configPath), config.source);
  const require = createRequire(import.meta.url), sharp = require('sharp');
  const sourceBytes = await readFile(sourcePath);
  const sourceSha256 = createHash('sha256').update(sourceBytes).digest('hex');
  if (sourceSha256 !== config.sourceSha256) throw new Error('Source image hash differs from the reviewed anchor configuration.');
  const { data, info } = await sharp(sourceBytes).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
  const aligned = alignRgba(data, info.width, info.height, config);
  const report = auditRgba(aligned, info.width, info.height, config.columns, config.rows);
  if (report.warnings.length) throw new Error(`Aligned atlas failed QA: ${report.warnings.join(' ')}`);
  const png = await sharp(aligned, { raw: { width: info.width, height: info.height, channels: 4 } }).png().toBuffer();
  // Exclusive outputs: never overwrite either source artwork or an earlier revision.
  await mkdir(outDir, { recursive: false });
  await writeFile(path.join(outDir, 'atlas.png'), png, { flag: 'wx' });
  await writeFile(path.join(outDir, 'review.json'), JSON.stringify({ sourceSha256, settings: config, report }, null, 2) + '\n', { flag: 'wx' });
  console.log(`Prepared ${config.columns * config.rows} frames; QA warnings: ${report.warnings.length}; output: ${outDir}`);
}
