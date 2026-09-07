// Uses an AI-extracted alpha mask, but keeps the reviewed original RGB unchanged.
import { createRequire } from 'node:module';
import { readFile, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { pathToFileURL } from 'node:url';
import path from 'node:path';

export function prepareCutout(original, mask, width, height, { paddingRight = 0, paddingBottom = 0, removeBrightBackdrop = false } = {}) {
  if (!Number.isInteger(width) || !Number.isInteger(height) || width <= 0 || height <= 0
      || original.length !== width * height * 4 || mask.length !== original.length
      || ![paddingRight, paddingBottom].every(n => Number.isInteger(n) && n >= 0 && n <= 1024))
    throw new Error('Matching RGBA inputs and bounded integer padding are required.');
  const backdrop = new Uint8Array(width * height);
  if (removeBrightBackdrop) {
    // Flood only neutral, bright pixels connected to the canvas boundary. Closed
    // ivory clothing is not a background region; dark outlines are never eroded.
    const queue = new Int32Array(width * height); let head = 0, tail = 0;
    const visit = p => {
      if (backdrop[p]) return;
      const i = p * 4, lo = Math.min(original[i], original[i + 1], original[i + 2]);
      const hi = Math.max(original[i], original[i + 1], original[i + 2]);
      if (lo < 225 || hi - lo > 12) return;
      backdrop[p] = 1; queue[tail++] = p;
    };
    for (let x = 0; x < width; x++) { visit(x); visit((height - 1) * width + x); }
    for (let y = 0; y < height; y++) { visit(y * width); visit(y * width + width - 1); }
    while (head < tail) {
      const p = queue[head++], x = p % width, y = Math.floor(p / width);
      if (x) visit(p - 1); if (x + 1 < width) visit(p + 1);
      if (y) visit(p - width); if (y + 1 < height) visit(p + width);
    }
  }
  const outputWidth = width + paddingRight, outputHeight = height + paddingBottom;
  const data = Buffer.alloc(outputWidth * outputHeight * 4);
  for (let y = 0; y < height; y++) for (let x = 0; x < width; x++) {
    const p = y * width + x, src = p * 4, dest = (y * outputWidth + x) * 4;
    if (backdrop[p] || mask[src + 3] < 128 || original[src + 3] < 128) continue;
    data.set(original.subarray(src, src + 3), dest); data[dest + 3] = 255;
  }
  return { data, width: outputWidth, height: outputHeight };
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  const [configFile, outputFile] = process.argv.slice(2);
  if (!configFile || !outputFile || process.argv.length !== 4) throw new Error('Usage: prepare_sprite_cutout.mjs config.json NEW-output.png');
  const configPath = path.resolve(configFile), config = JSON.parse(await readFile(configPath, 'utf8'));
  const sharp = createRequire(import.meta.url)('sharp');
  const inputs = [];
  for (const key of ['original', 'mask']) {
    const bytes = await readFile(path.resolve(path.dirname(configPath), config[key]));
    if (createHash('sha256').update(bytes).digest('hex') !== config[key + 'Sha256']) throw new Error(`${key} hash mismatch.`);
    inputs.push(await sharp(bytes).ensureAlpha().raw().toBuffer({ resolveWithObject: true }));
  }
  if (inputs[0].info.width !== inputs[1].info.width || inputs[0].info.height !== inputs[1].info.height)
    throw new Error('Mask extraction changed canvas dimensions.');
  const result = prepareCutout(inputs[0].data, inputs[1].data, inputs[0].info.width, inputs[0].info.height, config);
  const png = await sharp(result.data, { raw: { width: result.width, height: result.height, channels: 4 } }).png().toBuffer();
  await writeFile(outputFile, png, { flag: 'wx' });
  console.log(`Prepared original-color cutout ${result.width}x${result.height}; no poses resized or redrawn.`);
}
