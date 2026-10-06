#!/usr/bin/env node
// Regrids explicitly reviewed, non-uniform source rectangles. No resampling,
// automatic crop, new drawings, frame duplication or source-file changes.
// Optional componentSeeds explicitly assign ALL four-neighbour alpha components
// to frames. Components are never ranked, auto-selected or silently discarded.
import { createHash } from 'node:crypto';
import { createRequire } from 'node:module';
import { readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const MAX_PIXELS = 16 * 1024 * 1024;

export function validateSourceSha256(bytes, expected) {
  if (!(bytes instanceof Uint8Array) || typeof expected !== 'string' || !/^[a-f0-9]{64}$/i.test(expected))
    throw new Error('Supply the reviewed source SHA256 as 64 hexadecimal characters.');
  const actual = createHash('sha256').update(bytes).digest('hex');
  if (actual !== expected.toLowerCase()) throw new Error('Source image hash differs from the reviewed region configuration.');
  return actual;
}

function assignAuthoredComponents(data, width, height, alphaThreshold, sourceRects, componentSeeds) {
  if (!Array.isArray(componentSeeds) || componentSeeds.length !== 8
      || !componentSeeds.every(group => Array.isArray(group) && group.length > 0))
    throw new Error('componentSeeds requires eight non-empty groups of authored [x,y] source-pixel seeds.');
  for (let frame = 0; frame < componentSeeds.length; frame++) for (const seed of componentSeeds[frame]) {
    if (!Array.isArray(seed) || seed.length !== 2 || !seed.every(Number.isSafeInteger)
        || seed[0] < 0 || seed[0] >= width || seed[1] < 0 || seed[1] >= height)
      throw new Error(`Frame ${frame} has an invalid component seed coordinate.`);
    if (data[(seed[1] * width + seed[0]) * 4 + 3] < alphaThreshold)
      throw new Error(`Frame ${frame} component seed (${seed[0]},${seed[1]}) is not a visible source pixel.`);
  }

  const labels = new Int32Array(width * height);
  const queue = new Int32Array(width * height);
  const components = [];
  // Deterministic row-major labelling identifies connectivity only, not the
  // semantic frame. Only the human-authored seeds below determine ownership.
  for (let start = 0; start < labels.length; start++) {
    if (labels[start] || data[start * 4 + 3] < alphaThreshold) continue;
    const id = components.length + 1;
    const component = { id, pixelCount: 0, bounds: { left: width, top: height, right: -1, bottom: -1 } };
    let head = 0, tail = 1;
    queue[0] = start;
    labels[start] = id;
    const visit = pixel => {
      if (labels[pixel] || data[pixel * 4 + 3] < alphaThreshold) return;
      labels[pixel] = id;
      queue[tail++] = pixel;
    };
    while (head < tail) {
      const pixel = queue[head++], x = pixel % width, y = Math.floor(pixel / width);
      component.pixelCount++;
      component.bounds.left = Math.min(component.bounds.left, x);
      component.bounds.top = Math.min(component.bounds.top, y);
      component.bounds.right = Math.max(component.bounds.right, x);
      component.bounds.bottom = Math.max(component.bounds.bottom, y);
      if (x > 0) visit(pixel - 1);
      if (x + 1 < width) visit(pixel + 1);
      if (y > 0) visit(pixel - width);
      if (y + 1 < height) visit(pixel + width);
    }
    components.push(component);
  }

  const owners = new Int32Array(components.length + 1).fill(-1);
  const frameComponents = componentSeeds.map(() => []);
  for (let frame = 0; frame < componentSeeds.length; frame++) for (const [x, y] of componentSeeds[frame]) {
    const id = labels[y * width + x];
    if (owners[id] !== -1) {
      if (owners[id] === frame) throw new Error(`Frame ${frame} selects component ${id} more than once.`);
      throw new Error(`Component ${id} is selected by multiple frames ${owners[id]} and ${frame}.`);
    }
    const component = components[id - 1], bounds = component.bounds;
    const [rx, ry, rw, rh] = sourceRects[frame];
    if (bounds.left < rx || bounds.top < ry || bounds.right >= rx + rw || bounds.bottom >= ry + rh)
      throw new Error(`Frame ${frame} selected component ${id} is not completely inside its authored source rectangle.`);
    if (bounds.left === rx || bounds.top === ry || bounds.right === rx + rw - 1 || bounds.bottom === ry + rh - 1)
      throw new Error(`Frame ${frame} selected component ${id} touches a source rectangle boundary; retain the full component with margin.`);
    owners[id] = frame;
    frameComponents[frame].push({ id, pixelCount: component.pixelCount, sourceBounds: { ...bounds } });
  }
  for (const component of components) {
    if (owners[component.id] === -1)
      throw new Error(`Visible component ${component.id} (${component.pixelCount} pixels, first bounds at ${component.bounds.left},${component.bounds.top}) has no authored seed; isolated pixels must be explicitly assigned, not discarded.`);
  }
  return { labels, owners, frameComponents, componentCount: components.length };
}

export function prepareAuthoredRegions(data, width, height, settings) {
  if (!(data instanceof Uint8Array) || ![width, height].every(n => Number.isSafeInteger(n) && n > 0)
      || width * height > MAX_PIXELS || data.length !== width * height * 4
      || !settings || typeof settings !== 'object' || Array.isArray(settings))
    throw new Error('Supply bounded source RGBA dimensions and a region settings object.');
  const columns = settings.columns === undefined ? 4 : settings.columns;
  const rows = settings.rows === undefined ? 2 : settings.rows;
  const outputFrameSize = settings.outputFrameSize === undefined ? [512, 512] : settings.outputFrameSize;
  const alphaThreshold = settings.alphaThreshold === undefined ? 128 : settings.alphaThreshold;
  const sourceRects = settings.sourceRects;
  if (![columns, rows].every(n => Number.isInteger(n) && n > 0 && n <= 8)
      || columns * rows !== 8 || !Array.isArray(sourceRects) || sourceRects.length !== 8
      || !Array.isArray(outputFrameSize) || outputFrameSize.length !== 2
      || !outputFrameSize.every(n => Number.isInteger(n) && n > 0 && n <= 4096)
      || outputFrameSize[0] * outputFrameSize[1] * columns * rows > MAX_PIXELS
      || !Number.isInteger(alphaThreshold) || alphaThreshold < 1 || alphaThreshold > 255)
    throw new Error('Require eight rectangles, a matching eight-cell grid, bounded integer output size and alpha threshold 1..255.');
  const [frameWidth, frameHeight] = outputFrameSize;
  for (let frame = 0; frame < sourceRects.length; frame++) {
    const rect = sourceRects[frame];
    if (!Array.isArray(rect) || rect.length !== 4 || !rect.every(Number.isSafeInteger))
      throw new Error(`Frame ${frame} requires an integer [x,y,width,height] source rectangle.`);
    const [x, y, w, h] = rect;
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > width || y + h > height
        || w > frameWidth || h > frameHeight)
      throw new Error(`Frame ${frame} source rectangle must be inside the canvas and fit the output cell without resizing.`);
  }

  const components = settings.componentSeeds === undefined ? null
    : assignAuthoredComponents(data, width, height, alphaThreshold, sourceRects, settings.componentSeeds);

  const outputWidth = frameWidth * columns, outputHeight = frameHeight * rows;
  const output = Buffer.alloc(outputWidth * outputHeight * 4);
  const frames = sourceRects.map((rect, frame) => ({
    frame, sourceRect: [...rect], pixelCount: 0,
    bounds: { left: frameWidth, top: frameHeight, right: -1, bottom: -1 },
    sourceBounds: { left: width, top: height, right: -1, bottom: -1 },
    ...(components ? { componentSeeds: settings.componentSeeds[frame].map(seed => [...seed]),
      components: components.frameComponents[frame] } : {})
  }));
  let visiblePixelCount = 0;
  for (let y = 0; y < height; y++) for (let x = 0; x < width; x++) {
    const src = (y * width + x) * 4;
    if (data[src + 3] < alphaThreshold) continue;
    let matchedFrame = components ? components.owners[components.labels[y * width + x]] : -1;
    if (!components) {
      for (let frame = 0; frame < sourceRects.length; frame++) {
        const [rx, ry, rw, rh] = sourceRects[frame];
        if (x >= rx && x < rx + rw && y >= ry && y < ry + rh) {
          if (matchedFrame !== -1)
            throw new Error(`Visible pixel (${x},${y}) overlaps source rectangles ${matchedFrame} and ${frame}.`);
          matchedFrame = frame;
        }
      }
    }
    if (matchedFrame === -1) throw new Error(`Visible pixel (${x},${y}) is missing from every authored source rectangle.`);
    const [rx, ry, rw, rh] = sourceRects[matchedFrame];
    const localX = x - rx, localY = y - ry;
    if (localX === 0 || localY === 0 || localX === rw - 1 || localY === rh - 1)
      throw new Error(`Frame ${matchedFrame} visible pixel (${x},${y}) touches a source rectangle boundary; revise the region instead of cutting artwork.`);
    const destX = matchedFrame % columns * frameWidth + localX;
    const destY = Math.floor(matchedFrame / columns) * frameHeight + localY;
    const dest = (destY * outputWidth + destX) * 4;
    output.set(data.subarray(src, src + 3), dest);
    output[dest + 3] = 255;
    const report = frames[matchedFrame];
    report.pixelCount++;
    report.bounds.left = Math.min(report.bounds.left, localX);
    report.bounds.top = Math.min(report.bounds.top, localY);
    report.bounds.right = Math.max(report.bounds.right, localX);
    report.bounds.bottom = Math.max(report.bounds.bottom, localY);
    report.sourceBounds.left = Math.min(report.sourceBounds.left, x);
    report.sourceBounds.top = Math.min(report.sourceBounds.top, y);
    report.sourceBounds.right = Math.max(report.sourceBounds.right, x);
    report.sourceBounds.bottom = Math.max(report.sourceBounds.bottom, y);
    visiblePixelCount++;
  }
  for (const frame of frames) {
    if (!frame.pixelCount) throw new Error(`Frame ${frame.frame} contains no visible source pixels.`);
  }
  return {
    data: output, width: outputWidth, height: outputHeight,
    columns, rows, outputFrameSize: [...outputFrameSize], alphaThreshold,
    visiblePixelCount, frames,
    regionMode: components ? 'authored-four-neighbour-components' : 'authored-rectangles',
    ...(components ? { componentCount: components.componentCount } : {}),
    note: 'Authored regions/components were translated only; every visible source pixel was retained. No component ranking, resampling, anatomical anchoring or animation acceptance is implied.'
  };
}

export async function writeExclusiveOutput(outputFile, png) {
  await writeFile(outputFile, png, { flag: 'wx' });
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  const [configFile, outputFile] = process.argv.slice(2);
  if (!configFile || !outputFile || process.argv.length !== 4)
    throw new Error('Usage: prepare_authored_sprite_regions.mjs config.json NEW-output.png');
  const configPath = path.resolve(configFile);
  const config = JSON.parse(await readFile(configPath, 'utf8'));
  if (!config || typeof config !== 'object' || Array.isArray(config)
      || typeof config.source !== 'string' || !config.source.trim())
    throw new Error('The configuration requires a source image path.');
  const sourcePath = path.resolve(path.dirname(configPath), config.source);
  const sourceBytes = await readFile(sourcePath);
  const sourceSha256 = validateSourceSha256(sourceBytes, config.sourceSha256);
  const sharp = createRequire(import.meta.url)('sharp');
  const { data, info } = await sharp(sourceBytes).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
  const result = prepareAuthoredRegions(data, info.width, info.height, config);
  const png = await sharp(result.data, { raw: { width: result.width, height: result.height, channels: 4 } }).png().toBuffer();
  await writeExclusiveOutput(path.resolve(outputFile), png);
  const { data: _pixels, ...report } = result;
  console.log(JSON.stringify({
    source: sourcePath, sourceSha256,
    sourceSize: [info.width, info.height], output: path.resolve(outputFile),
    outputSha256: createHash('sha256').update(png).digest('hex'), ...report
  }, null, 2));
}
