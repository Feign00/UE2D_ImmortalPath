// Pure cell assembly from an explicit human-reviewed frame map. No drawing,
// registration, scaling, interpolation, alpha cleanup or automatic selection.
import { createRequire } from 'node:module';
import { readFile, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { pathToFileURL } from 'node:url';
import path from 'node:path';

const require = createRequire(import.meta.url);
const sharp = require('sharp');
const sha256 = bytes => createHash('sha256').update(bytes).digest('hex');
const hashPattern = /^[0-9a-f]{64}$/i;
const pngSignature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
const maxPixels = 16 * 1024 * 1024;

export function validateAssemblyConfig(config) {
  if (!config || typeof config !== 'object' || Array.isArray(config)) {
    throw new Error('Supply a JSON object with sources, grid dimensions and an explicit eight-entry frameMap.');
  }
  const { sources, columns, rows, cellWidth, cellHeight, frameMap } = config;
  if (columns !== 4 || rows !== 2) throw new Error('This reviewed animation assembler requires a 4x2, eight-cell grid.');
  if (![cellWidth, cellHeight].every(value => Number.isInteger(value) && value > 0 && value <= 4096)
      || columns * rows * cellWidth * cellHeight > maxPixels) {
    throw new Error('Supply positive integer cellWidth/cellHeight within the pixel budget.');
  }
  if (!Array.isArray(sources) || sources.length < 1 || sources.length > 16) {
    throw new Error('Supply between one and sixteen preserved source atlases.');
  }
  for (let index = 0; index < sources.length; index++) {
    const source = sources[index];
    if (!source || typeof source.path !== 'string' || source.path.trim().length === 0
        || source.path.includes('\0') || /^[a-z]+:\/\//i.test(source.path)
        || typeof source.sha256 !== 'string' || !hashPattern.test(source.sha256)) {
      throw new Error(`Source ${index} requires a local path and a complete reviewed SHA-256.`);
    }
  }
  if (!Array.isArray(frameMap) || frameMap.length !== 8) throw new Error('frameMap must explicitly select all eight output frames.');
  const pairs = new Set();
  for (let output = 0; output < frameMap.length; output++) {
    const pair = frameMap[output];
    if (!pair || !Number.isInteger(pair.source) || pair.source < 0 || pair.source >= sources.length
        || !Number.isInteger(pair.frame) || pair.frame < 0 || pair.frame >= 8) {
      throw new Error(`Output frame ${output} has an out-of-range source/frame pair.`);
    }
    const key = `${pair.source}:${pair.frame}`;
    if (pairs.has(key)) throw new Error(`Duplicate manual source/frame pair ${key}.`);
    pairs.add(key);
  }
  return config;
}

/** Decodes only verified 8-bit RGBA PNGs. Never adds alpha or resizes a source. */
export async function loadReviewedSources(config, configFile) {
  validateAssemblyConfig(config);
  const directory = path.dirname(path.resolve(configFile));
  const loaded = [];
  for (let index = 0; index < config.sources.length; index++) {
    const reviewed = config.sources[index];
    const sourcePath = path.resolve(directory, reviewed.path);
    const bytes = await readFile(sourcePath);
    const fileSha256 = sha256(bytes);
    if (fileSha256 !== reviewed.sha256.toLowerCase()) throw new Error(`Source ${index} hash differs from the reviewed configuration.`);
    // PNG IHDR must be true 8-bit RGBA, not RGB, indexed colour, grayscale,
    // or an implicit alpha conversion that could hide an incorrect input.
    if (bytes.length < 33 || !bytes.subarray(0, 8).equals(pngSignature)
        || bytes.subarray(12, 16).toString('ascii') !== 'IHDR' || bytes[24] !== 8 || bytes[25] !== 6) {
      throw new Error(`Source ${index} must be a true 8-bit RGBA PNG.`);
    }
    const metadata = await sharp(bytes, { limitInputPixels: maxPixels }).metadata();
    const width = config.columns * config.cellWidth;
    const height = config.rows * config.cellHeight;
    if (metadata.width !== width || metadata.height !== height) {
      throw new Error(`Source ${index} dimensions ${metadata.width}x${metadata.height} differ from the complete ${width}x${height} reviewed grid.`);
    }
    if (metadata.channels !== 4 || !metadata.hasAlpha || metadata.depth !== 'uchar') {
      throw new Error(`Source ${index} does not decode as 8-bit RGBA.`);
    }
    const { data, info } = await sharp(bytes, { limitInputPixels: maxPixels }).raw().toBuffer({ resolveWithObject: true });
    if (info.width !== width || info.height !== height || info.channels !== 4 || info.depth !== 'uchar') {
      throw new Error(`Source ${index} decoder changed the expected RGBA layout.`);
    }
    loaded.push({ path: sourcePath, sha256: fileSha256, data, width, height, channels: 4 });
  }
  return loaded;
}

/** Copies all selected cell bytes, including transparent RGB and partial alpha. */
export function assembleReviewedRgba(config, sources) {
  validateAssemblyConfig(config);
  if (!Array.isArray(sources) || sources.length !== config.sources.length) throw new Error('Decoded source count does not match the reviewed config.');
  const width = config.columns * config.cellWidth;
  const height = config.rows * config.cellHeight;
  const sourceManifest = [];
  for (let index = 0; index < sources.length; index++) {
    const source = sources[index];
    if (!source || source.width !== width || source.height !== height || source.channels !== 4
        || !Buffer.isBuffer(source.data) || source.data.length !== width * height * 4) {
      throw new Error(`Decoded source ${index} does not contain the complete, same-size RGBA grid.`);
    }
    if (typeof source.sha256 !== 'string' || source.sha256.toLowerCase() !== config.sources[index].sha256.toLowerCase()) {
      throw new Error(`Decoded source ${index} file hash does not match the reviewed source.`);
    }
    sourceManifest.push({
      source: index,
      path: source.path ?? config.sources[index].path,
      sha256: source.sha256.toLowerCase(),
      rawRgbaSha256: sha256(source.data),
      width,
      height,
      channels: 4,
    });
  }
  const output = Buffer.alloc(width * height * 4);
  const manifest = [];
  const encodedPairs = new Set();
  const rawPairs = new Set();
  const selectedPixelHashes = new Set();
  const rowBytes = config.cellWidth * 4;
  for (let destinationFrame = 0; destinationFrame < 8; destinationFrame++) {
    const pair = config.frameMap[destinationFrame];
    const source = sources[pair.source];
    const provenance = sourceManifest[pair.source];
    const filePair = `${provenance.sha256}:${pair.frame}`;
    const rawPair = `${provenance.rawRgbaSha256}:${pair.frame}`;
    if (encodedPairs.has(filePair) || rawPairs.has(rawPair)) {
      throw new Error(`Output frame ${destinationFrame} repeats the same source content/frame through a source alias.`);
    }
    encodedPairs.add(filePair);
    rawPairs.add(rawPair);
    const selected = Buffer.alloc(config.cellWidth * config.cellHeight * 4);
    const sourceX = pair.frame % config.columns * config.cellWidth;
    const sourceY = Math.floor(pair.frame / config.columns) * config.cellHeight;
    const outputX = destinationFrame % config.columns * config.cellWidth;
    const outputY = Math.floor(destinationFrame / config.columns) * config.cellHeight;
    for (let row = 0; row < config.cellHeight; row++) {
      const start = ((sourceY + row) * width + sourceX) * 4;
      const cellRow = source.data.subarray(start, start + rowBytes);
      selected.set(cellRow, row * rowBytes);
      output.set(cellRow, ((outputY + row) * width + outputX) * 4);
    }
    let visiblePixels = 0;
    for (let alpha = 3; alpha < selected.length; alpha += 4) if (selected[alpha] !== 0) visiblePixels++;
    if (visiblePixels === 0) throw new Error(`Output frame ${destinationFrame} selects an empty transparent frame.`);
    const rgbaSha256 = sha256(selected);
    if (selectedPixelHashes.has(rgbaSha256)) throw new Error(`Output frame ${destinationFrame} duplicates a selected frame's exact RGBA bytes.`);
    selectedPixelHashes.add(rgbaSha256);
    manifest.push({
      outputFrame: destinationFrame,
      source: pair.source,
      frame: pair.frame,
      sourceSha256: provenance.sha256,
      sourceRawRgbaSha256: provenance.rawRgbaSha256,
      sourceRegion: [sourceX, sourceY, config.cellWidth, config.cellHeight],
      outputRegion: [outputX, outputY, config.cellWidth, config.cellHeight],
      frameRgbaSha256: rgbaSha256,
      visiblePixels,
    });
  }
  return {
    data: output,
    width,
    height,
    manifest: {
      operation: 'explicit-reviewed-frame-cell-copy',
      columns: config.columns,
      rows: config.rows,
      cellWidth: config.cellWidth,
      cellHeight: config.cellHeight,
      frameCount: 8,
      sources: sourceManifest,
      frames: manifest,
      outputRawRgbaSha256: sha256(output),
      acceptanceLimits: 'Pixel-preserving manual selection only. No drawing, alignment, interpolation, automatic selection, pose continuity, foot-anchor or animation-fluidity acceptance.',
    },
  };
}

export async function assembleFile(configFile, newOutputFile) {
  const configPath = path.resolve(configFile);
  const outputPath = path.resolve(newOutputFile);
  if (path.extname(outputPath).toLowerCase() !== '.png') throw new Error('NEW output must be a .png file.');
  const configBytes = await readFile(configPath);
  const config = JSON.parse(configBytes.toString('utf8'));
  const sources = await loadReviewedSources(config, configPath);
  const assembly = assembleReviewedRgba(config, sources);
  const png = await sharp(assembly.data, { raw: { width: assembly.width, height: assembly.height, channels: 4 } }).png().toBuffer();
  // Exclusive creation protects originals, prior revisions and concurrent runs.
  // The caller must prepare the destination directory; no source path is written.
  await writeFile(outputPath, png, { flag: 'wx' });
  return {
    configPath,
    configSha256: sha256(configBytes),
    outputPath,
    outputSha256: sha256(png),
    ...assembly.manifest,
  };
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  const [configFile, outputFile] = process.argv.slice(2);
  if (!configFile || !outputFile || process.argv.length !== 4) {
    throw new Error('Usage: assemble_reviewed_sprite_atlas.mjs reviewed-config.json NEW-output.png');
  }
  const report = await assembleFile(configFile, outputFile);
  console.log(JSON.stringify(report, null, 2));
}
