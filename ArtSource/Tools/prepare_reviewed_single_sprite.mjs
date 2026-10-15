// Prepares one explicitly reviewed AI frame for manual atlas assembly. The
// complete square canvas is normalized uniformly; actor bounds never set scale.
import { createRequire } from 'node:module';
import { readFile, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { pathToFileURL } from 'node:url';
import path from 'node:path';

const sharp = createRequire(import.meta.url)('sharp');
const sha256 = bytes => createHash('sha256').update(bytes).digest('hex');
const hashPattern = /^[0-9a-f]{64}$/i;
const pngSignature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
const maxPixels = 16 * 1024 * 1024;
const columns = 4, rows = 2, normalizedSize = 512, alphaThreshold = 128;

function isObject(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

/** No bbox, per-axis scale, inferred frame, or automatic anchor options exist. */
export function validateSingleSpriteConfig(config) {
  if (!isObject(config)) throw new Error('Supply a JSON object with a reviewed source and explicit outputFrame.');
  const allowed = new Set(['source', 'expectedSourceSize', 'normalizedCanvasSize', 'outputFrame']);
  if (Object.keys(config).some(key => !allowed.has(key))) throw new Error('Unsupported configuration option; only whole-canvas normalization is allowed.');
  if (!isObject(config.source) || Object.keys(config.source).some(key => !['path', 'sha256'].includes(key))
      || typeof config.source.path !== 'string' || !config.source.path.trim()
      || config.source.path.includes('\0') || /^[a-z]+:\/\//i.test(config.source.path)
      || typeof config.source.sha256 !== 'string' || !hashPattern.test(config.source.sha256)) {
    throw new Error('source requires only a local path and complete reviewed SHA-256.');
  }
  if (!Number.isInteger(config.expectedSourceSize) || config.expectedSourceSize < 3
      || config.expectedSourceSize > 4096 || config.expectedSourceSize ** 2 > maxPixels) {
    throw new Error('expectedSourceSize must be the square canvas edge, an integer from 3 through 4096.');
  }
  if (config.normalizedCanvasSize !== normalizedSize) throw new Error('normalizedCanvasSize must be exactly 512.');
  if (!Number.isInteger(config.outputFrame) || config.outputFrame < 0 || config.outputFrame > 7) {
    throw new Error('outputFrame must explicitly name one integer cell from 0 through 7.');
  }
  return config;
}

/** Verify encoded provenance and true RGBA without adding alpha or cropping. */
export async function loadReviewedSingleSource(config, configFile) {
  validateSingleSpriteConfig(config);
  const sourcePath = path.resolve(path.dirname(path.resolve(configFile)), config.source.path);
  const bytes = await readFile(sourcePath);
  const fileSha256 = sha256(bytes);
  if (fileSha256 !== config.source.sha256.toLowerCase()) throw new Error('Source hash differs from the reviewed configuration.');
  if (bytes.length < 33 || !bytes.subarray(0, 8).equals(pngSignature)
      || bytes.subarray(12, 16).toString('ascii') !== 'IHDR' || bytes[24] !== 8 || bytes[25] !== 6) {
    throw new Error('Source must be a true 8-bit RGBA PNG.');
  }
  const metadata = await sharp(bytes, { limitInputPixels: maxPixels }).metadata();
  const size = config.expectedSourceSize;
  if (metadata.width !== size || metadata.height !== size) {
    throw new Error(`Source dimensions ${metadata.width}x${metadata.height} differ from the reviewed square ${size}x${size} canvas.`);
  }
  if (metadata.channels !== 4 || !metadata.hasAlpha || metadata.depth !== 'uchar') {
    throw new Error('Source does not decode as 8-bit RGBA.');
  }
  const { data, info } = await sharp(bytes, { limitInputPixels: maxPixels }).raw().toBuffer({ resolveWithObject: true });
  if (info.width !== size || info.height !== size || info.channels !== 4 || info.depth !== 'uchar') {
    throw new Error('Source decoder changed the expected RGBA layout.');
  }
  return { path: sourcePath, sha256: fileSha256, width: size, height: size, channels: 4, data };
}

function visibleExtent(data, width, height, threshold) {
  let count = 0, minX = width, minY = height, maxX = -1, maxY = -1;
  for (let y = 0; y < height; y++) for (let x = 0; x < width; x++) {
    if (data[(y * width + x) * 4 + 3] < threshold) continue;
    count++;
    minX = Math.min(minX, x); minY = Math.min(minY, y);
    maxX = Math.max(maxX, x); maxY = Math.max(maxY, y);
  }
  return { pixels: count, bounds: count ? [minX, minY, maxX + 1, maxY + 1] : null };
}

/** Threshold first, then nearest sample the full square canvas at pixel centers. */
export function prepareReviewedSingleRgba(config, source) {
  validateSingleSpriteConfig(config);
  const size = config.expectedSourceSize;
  if (!source || source.width !== size || source.height !== size || source.channels !== 4
      || !Buffer.isBuffer(source.data) || source.data.length !== size * size * 4) {
    throw new Error('Decoded source must contain the complete reviewed square RGBA canvas.');
  }
  if (typeof source.sha256 !== 'string' || source.sha256.toLowerCase() !== config.source.sha256.toLowerCase()) {
    throw new Error('Decoded source file hash does not match the reviewed source.');
  }
  const sourceVisible = visibleExtent(source.data, size, size, alphaThreshold);
  if (!sourceVisible.pixels) throw new Error('Source has no visible pixels at alpha >= 128.');
  const [left, top, right, bottom] = sourceVisible.bounds;
  if (left === 0 || top === 0 || right === size || bottom === size) {
    throw new Error('Source visible pixels at alpha >= 128 touch a canvas edge; request an unclipped AI frame.');
  }

  // Never mutate source.data. Hidden RGB is zeroed together with discarded
  // alpha; accepted source RGB is copied verbatim and its alpha becomes 255.
  const thresholded = Buffer.alloc(source.data.length);
  for (let offset = 0; offset < source.data.length; offset += 4) {
    if (source.data[offset + 3] < alphaThreshold) continue;
    source.data.copy(thresholded, offset, offset, offset + 3);
    thresholded[offset + 3] = 255;
  }
  const normalized = Buffer.alloc(normalizedSize * normalizedSize * 4);
  for (let y = 0; y < normalizedSize; y++) for (let x = 0; x < normalizedSize; x++) {
    const sourceX = Math.min(size - 1, Math.floor((x + 0.5) * size / normalizedSize));
    const sourceY = Math.min(size - 1, Math.floor((y + 0.5) * size / normalizedSize));
    const sourceOffset = (sourceY * size + sourceX) * 4;
    thresholded.copy(normalized, (y * normalizedSize + x) * 4, sourceOffset, sourceOffset + 4);
  }
  const normalizedVisible = visibleExtent(normalized, normalizedSize, normalizedSize, 255);
  if (!normalizedVisible.pixels) throw new Error('Whole-canvas nearest normalization leaves an empty frame; no pixels will be invented.');

  const width = columns * normalizedSize, height = rows * normalizedSize;
  const output = Buffer.alloc(width * height * 4);
  const x = config.outputFrame % columns * normalizedSize;
  const y = Math.floor(config.outputFrame / columns) * normalizedSize;
  for (let row = 0; row < normalizedSize; row++) {
    normalized.copy(output, ((y + row) * width + x) * 4, row * normalizedSize * 4, (row + 1) * normalizedSize * 4);
  }
  return {
    data: output, width, height,
    manifest: {
      operation: 'reviewed-single-frame-whole-canvas-nearest-normalization',
      source: {
        path: source.path ?? config.source.path,
        sha256: source.sha256.toLowerCase(),
        rawRgbaSha256: sha256(source.data),
        width: size, height: size, channels: 4,
        visibleAlphaThreshold: alphaThreshold,
        visiblePixels: sourceVisible.pixels,
        visibleBoundsExclusive: sourceVisible.bounds,
      },
      normalization: {
        sourceRegion: [0, 0, size, size],
        normalizedCanvasSize: normalizedSize,
        uniformScale: { numerator: normalizedSize, denominator: size, value: normalizedSize / size },
        alphaOperation: 'source alpha >= 128 becomes 255, all other RGBA becomes zero before sampling',
        kernel: 'nearest',
        pixelMapping: 'sourceIndex = floor((destinationIndex + 0.5) * sourceSize / 512), equal on both axes',
        thresholdedRawRgbaSha256: sha256(thresholded),
        normalizedRawRgbaSha256: sha256(normalized),
        visiblePixels: normalizedVisible.pixels,
        visibleBoundsExclusive: normalizedVisible.bounds,
      },
      columns, rows, cellWidth: normalizedSize, cellHeight: normalizedSize,
      gridCellCount: 8,
      nonemptyFrameCount: 1,
      outputFrame: config.outputFrame,
      populatedFrames: [config.outputFrame],
      emptyFrames: Array.from({ length: 8 }, (_, frame) => frame).filter(frame => frame !== config.outputFrame),
      outputRegion: [x, y, normalizedSize, normalizedSize],
      outputRawRgbaSha256: sha256(output),
      acceptanceLimits: 'One AI frame only, not an eight-frame animation. The populated cell requires explicit human frameMap selection; the other seven cells are empty and invalid to select. No bbox scaling, per-axis scaling, drawing, interpolation, inserted poses, automatic anchor, pose continuity or animation-fluidity acceptance. Existing later whole-clip scale is not changed.',
    },
  };
}

export async function prepareSingleSpriteFile(configFile, newOutputFile) {
  const configPath = path.resolve(configFile), outputPath = path.resolve(newOutputFile);
  if (path.extname(outputPath).toLowerCase() !== '.png') throw new Error('NEW output must be a .png file.');
  const configBytes = await readFile(configPath);
  const config = JSON.parse(configBytes.toString('utf8'));
  const source = await loadReviewedSingleSource(config, configPath);
  const prepared = prepareReviewedSingleRgba(config, source);
  const png = await sharp(prepared.data, { raw: { width: prepared.width, height: prepared.height, channels: 4 } }).png().toBuffer();
  // The caller prepares the output directory. Exclusive creation also protects
  // the original source and every previously produced revision.
  await writeFile(outputPath, png, { flag: 'wx' });
  return { configPath, configSha256: sha256(configBytes), outputPath, outputSha256: sha256(png), ...prepared.manifest };
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  const [configFile, outputFile] = process.argv.slice(2);
  if (!configFile || !outputFile || process.argv.length !== 4) {
    throw new Error('Usage: prepare_reviewed_single_sprite.mjs reviewed-config.json NEW-output.png');
  }
  console.log(JSON.stringify(await prepareSingleSpriteFile(configFile, outputFile), null, 2));
}
