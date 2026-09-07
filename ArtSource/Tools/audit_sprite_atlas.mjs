#!/usr/bin/env node
// Read-only atlas checks. This tool never retouches or rescales artwork.
import { createHash } from 'node:crypto';
import { createRequire } from 'node:module';
import { pathToFileURL } from 'node:url';
import path from 'node:path';
import { parseArgs } from 'node:util';

export function auditRgba(data, width, height, columns, rows, options = {}) {
  const { alphaThreshold = 128, bottomTolerance = 2 } = options;
  if (![width, height, columns, rows].every(n => Number.isSafeInteger(n) && n > 0)
      || width % columns || height % rows || data.length !== width * height * 4) {
    throw new Error('RGBA dimensions must divide exactly into a positive grid.');
  }
  if (!Number.isInteger(alphaThreshold) || alphaThreshold < 1 || alphaThreshold > 255
      || !Number.isFinite(bottomTolerance) || bottomTolerance < 0) {
    throw new Error('Alpha threshold must be 1..255; bottom tolerance must be non-negative.');
  }
  const cellWidth = width / columns, cellHeight = height / rows;
  const frames = [];
  for (let frame = 0; frame < columns * rows; frame++) {
    let clear = 0, partial = 0, opaque = 0, solid = 0, maxAlpha = 0;
    let left = cellWidth, top = cellHeight, right = -1, bottom = -1;
    const pixels = Buffer.alloc(cellWidth * cellHeight * 4);
    const maskedPixels = Buffer.alloc(pixels.length);
    for (let y = 0; y < cellHeight; y++) {
      for (let x = 0; x < cellWidth; x++) {
        const offset = ((Math.floor(frame / columns) * cellHeight + y) * width
          + (frame % columns) * cellWidth + x) * 4;
        const alpha = data[offset + 3];
        maxAlpha = Math.max(maxAlpha, alpha);
        pixels.set(data.subarray(offset, offset + 4), (y * cellWidth + x) * 4);
        if (alpha === 0) clear++; else if (alpha === 255) opaque++; else partial++;
        if (alpha >= alphaThreshold) {
          solid++;
          const target = (y * cellWidth + x) * 4;
          maskedPixels.set(data.subarray(offset, offset + 3), target);
          maskedPixels[target + 3] = 255;
          left = Math.min(left, x); right = Math.max(right, x);
          top = Math.min(top, y); bottom = Math.max(bottom, y);
        }
      }
    }
    const bounds = solid ? { left, top, right, bottom } : null;
    frames.push({ frame, clear, partial, opaque, solid, maxAlpha, bounds,
      touchesCellEdge: !!bounds && (left === 0 || top === 0 || right === cellWidth - 1 || bottom === cellHeight - 1),
      maskedSha256: createHash('sha256').update(maskedPixels).digest('hex'),
      sha256: createHash('sha256').update(pixels).digest('hex') });
  }
  const bottoms = frames.filter(f => f.bounds).map(f => f.bounds.bottom);
  const warnings = [];
  if (frames.some(f => !f.clear)) warnings.push('Some frames lack fully transparent pixels (possible baked backdrop).');
  if (frames.some(f => !f.solid)) warnings.push('Some frames have no visible masked silhouette.');
  if (frames.some(f => f.solid && !f.opaque)) warnings.push('Some visible frames have no fully opaque pixels; verify alpha/material before import.');
  if (frames.some(f => f.touchesCellEdge)) warnings.push('Some silhouettes touch a cell boundary.');
  if (new Set(frames.map(f => f.sha256)).size < frames.length) warnings.push('Pixel-identical frames detected; inspect intended timing.');
  else if (new Set(frames.map(f => f.maskedSha256)).size < frames.length) warnings.push('Masked-identical frames detected despite different hidden/alpha pixels.');
  const silhouetteBottomSpread = bottoms.length ? Math.max(...bottoms) - Math.min(...bottoms) : null;
  if (silhouetteBottomSpread > bottomTolerance) warnings.push('Silhouette bottom drift exceeds tolerance; review grounded foot anchors (not automatic correction).');
  return { width, height, columns, rows, cellWidth, cellHeight, alphaThreshold, bottomTolerance, frames,
    silhouetteBottomSpread,
    warnings, note: 'Bounds are not anatomical foot anchors. Human visual/animation review is required.' };
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  const { values, positionals } = parseArgs({ allowPositionals: true, options: {
    strict: { type: 'boolean', default: false },
    threshold: { type: 'string', default: '128' },
    'bottom-tolerance': { type: 'string', default: '2' }
  } });
  const [file, columns, rows] = positionals;
  if (positionals.length !== 3) throw new Error('Usage: audit_sprite_atlas.mjs image.png columns rows [--strict] [--threshold 128] [--bottom-tolerance 2]');
  const require = createRequire(import.meta.url);
  const sharp = require('sharp');
  const { data, info } = await sharp(file).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
  const report = auditRgba(data, info.width, info.height, Number(columns), Number(rows), {
    alphaThreshold: Number(values.threshold), bottomTolerance: Number(values['bottom-tolerance'])
  });
  console.log(JSON.stringify(report, null, 2));
  if (values.strict && report.warnings.length) process.exitCode = 2;
}
