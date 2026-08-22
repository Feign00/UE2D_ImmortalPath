#!/usr/bin/env node

import { createRequire } from "node:module";
import fs from "node:fs/promises";
import path from "node:path";

const require = createRequire(import.meta.url);
const sharp = require("sharp");

function fail(message) {
  console.error(`Error: ${message}`);
  process.exit(1);
}

function parseArguments(argv) {
  const positional = [];
  const options = new Map();

  for (let index = 0; index < argv.length; ++index) {
    const value = argv[index];
    if (!value.startsWith("--")) {
      positional.push(value);
      continue;
    }

    if (index + 1 >= argv.length) {
      fail(`Missing value for ${value}.`);
    }
    options.set(value.slice(2), argv[++index]);
  }

  if (positional.length !== 2) {
    fail(
      "Usage: prepare_chroma_sprite_sheet.mjs <input.png> <output-dir> " +
        "--columns 4 --rows 2 --width 512 --height 512 --foot-y 448 " +
        "--target-height 400 --key auto",
    );
  }

  const integer = (name, fallback) => {
    const parsed = Number.parseInt(options.get(name) ?? `${fallback}`, 10);
    if (!Number.isInteger(parsed) || parsed <= 0) {
      fail(`--${name} must be a positive integer.`);
    }
    return parsed;
  };

  const keyText = (options.get("key") ?? "auto").replace(/^#/, "");
  if (keyText.toLowerCase() !== "auto" && !/^[0-9a-fA-F]{6}$/.test(keyText)) {
    fail("--key must be 'auto' or a six-digit RGB hex value.");
  }
  const layout = (options.get("layout") ?? "grid").toLowerCase();
  if (!new Set(["grid", "components"]).has(layout)) {
    fail("--layout must be 'grid' or 'components'.");
  }

  return {
    input: path.resolve(positional[0]),
    outputDirectory: path.resolve(positional[1]),
    columns: integer("columns", 4),
    rows: integer("rows", 2),
    width: integer("width", 512),
    height: integer("height", 512),
    footY: integer("foot-y", 448),
    targetHeight: integer("target-height", 400),
    layout,
    key:
      keyText.toLowerCase() === "auto"
        ? null
        : [
            Number.parseInt(keyText.slice(0, 2), 16),
            Number.parseInt(keyText.slice(2, 4), 16),
            Number.parseInt(keyText.slice(4, 6), 16),
          ],
  };
}

function clampByte(value) {
  return Math.max(0, Math.min(255, Math.round(value)));
}

function smoothstep(value) {
  const clamped = Math.max(0, Math.min(1, value));
  return clamped * clamped * (3 - 2 * clamped);
}

function extractAlpha(rgba, key) {
  const result = Buffer.from(rgba);
  let opaquePixels = 0;
  const keyMaximum = Math.max(...key);
  const spillChannels = key
    .map((value, index) => ({ value, index }))
    .filter(({ value }) => value >= 128 && value >= keyMaximum - 16)
    .map(({ index }) => index);
  const nonSpillChannels = [0, 1, 2].filter((index) => !spillChannels.includes(index));

  for (let offset = 0; offset < result.length; offset += 4) {
    const red = result[offset];
    const green = result[offset + 1];
    const blue = result[offset + 2];
    const channels = [red, green, blue];
    const sourceAlpha = result[offset + 3];
    const distance = Math.max(
      Math.abs(red - key[0]),
      Math.abs(green - key[1]),
      Math.abs(blue - key[2]),
    );

    const keyStrength =
      spillChannels.length > 1
        ? Math.min(...spillChannels.map((channel) => channels[channel]))
        : spillChannels.length === 1
          ? channels[spillChannels[0]]
          : 0;
    const nonKeyStrength =
      nonSpillChannels.length > 0
        ? Math.max(...nonSpillChannels.map((channel) => channels[channel]))
        : 0;
    const dominance = keyStrength - nonKeyStrength;
    const keyLike = distance <= 32 || dominance >= 16;
    let outputAlpha = 255;

    if (keyLike) {
      const softAlpha =
        distance <= 12
          ? 0
          : distance >= 220
            ? 255
            : clampByte(255 * smoothstep((distance - 12) / 208));
      const dominanceAlpha =
        dominance <= 0
          ? 255
          : clampByte(
              255 *
                (1 - Math.min(1, dominance / Math.max(1, Math.max(...key) - nonKeyStrength))),
            );
      outputAlpha = Math.min(softAlpha, dominanceAlpha);
    }

    outputAlpha = clampByte(outputAlpha * (sourceAlpha / 255));
    if (outputAlpha <= 8) {
      outputAlpha = 0;
    }

    if (outputAlpha > 0 && outputAlpha < 252 && spillChannels.length > 0) {
      const neutralEdge = Math.max(0, nonKeyStrength - 1);
      for (const channel of spillChannels) {
        result[offset + channel] = Math.min(result[offset + channel], neutralEdge);
      }
    }

    result[offset + 3] = outputAlpha;
    if (outputAlpha > 8) {
      ++opaquePixels;
    }
  }

  return { data: result, opaquePixels };
}

function sampleBorderKey(rgba, width, height) {
  const channels = [[], [], []];
  const borderDepth = Math.min(4, Math.max(1, Math.floor(Math.min(width, height) / 16)));
  const add = (x, y) => {
    const offset = (y * width + x) * 4;
    for (let channel = 0; channel < 3; ++channel) {
      channels[channel].push(rgba[offset + channel]);
    }
  };

  for (let inset = 0; inset < borderDepth; ++inset) {
    for (let x = inset; x < width - inset; ++x) {
      add(x, inset);
      add(x, height - 1 - inset);
    }
    for (let y = inset + 1; y < height - 1 - inset; ++y) {
      add(inset, y);
      add(width - 1 - inset, y);
    }
  }

  return channels.map((values) => {
    values.sort((left, right) => left - right);
    return values[Math.floor(values.length / 2)];
  });
}

function labelAlphaComponents(rgba, width, height) {
  const pixelCount = width * height;
  const labels = new Int32Array(pixelCount);
  const queue = new Int32Array(pixelCount);
  const components = [];
  let componentLabel = 0;

  for (let start = 0; start < pixelCount; ++start) {
    if (labels[start] !== 0 || rgba[start * 4 + 3] <= 8) {
      continue;
    }
    ++componentLabel;
    let head = 0;
    let tail = 0;
    let size = 0;
    let minX = width;
    let minY = height;
    let maxX = -1;
    let maxY = -1;
    queue[tail++] = start;
    labels[start] = componentLabel;
    while (head < tail) {
      const current = queue[head++];
      ++size;
      const x = current % width;
      const y = Math.floor(current / width);
      minX = Math.min(minX, x);
      minY = Math.min(minY, y);
      maxX = Math.max(maxX, x);
      maxY = Math.max(maxY, y);
      const neighbors = [
        x + 1 < width ? current + 1 : -1,
        x > 0 ? current - 1 : -1,
        y + 1 < height ? current + width : -1,
        y > 0 ? current - width : -1,
      ];
      for (const neighbor of neighbors) {
        if (
          neighbor >= 0 &&
          labels[neighbor] === 0 &&
          rgba[neighbor * 4 + 3] > 8
        ) {
          labels[neighbor] = componentLabel;
          queue[tail++] = neighbor;
        }
      }
    }
    components.push({
      label: componentLabel,
      size,
      bounds: {
        left: minX,
        top: minY,
        width: maxX - minX + 1,
        height: maxY - minY + 1,
      },
    });
  }

  return { labels, components };
}

function removeSmallAlphaIslands(rgba, width, height) {
  const { labels, components } = labelAlphaComponents(rgba, width, height);

  if (components.length === 0) {
    return 0;
  }
  const largest = Math.max(...components.map((component) => component.size));
  const minimumSize = Math.max(128, Math.ceil(largest * 0.002));
  const keep = components.map((component) => component.size >= minimumSize);
  let remaining = 0;
  for (let pixel = 0; pixel < width * height; ++pixel) {
    const label = labels[pixel];
    if (label === 0) {
      continue;
    }
    if (!keep[label - 1]) {
      rgba[pixel * 4 + 3] = 0;
    } else {
      ++remaining;
    }
  }
  return remaining;
}

function boundsDistance(left, right) {
  const leftRight = left.left + left.width - 1;
  const leftBottom = left.top + left.height - 1;
  const rightRight = right.left + right.width - 1;
  const rightBottom = right.top + right.height - 1;
  const dx = Math.max(right.left - leftRight, left.left - rightRight, 0);
  const dy = Math.max(right.top - leftBottom, left.top - rightBottom, 0);
  return Math.hypot(dx, dy);
}

function buildComponentFrames(rgba, width, height, columns, rows) {
  const expectedFrames = columns * rows;
  const { labels, components } = labelAlphaComponents(rgba, width, height);
  const seeds = [...components]
    .filter((component) => component.size >= 1000)
    .sort((left, right) => right.size - left.size)
    .slice(0, expectedFrames);
  if (seeds.length !== expectedFrames) {
    fail(
      `Component layout expected ${expectedFrames} main figures but found ${seeds.length}.`,
    );
  }

  seeds.sort((left, right) => {
    const leftY = left.bounds.top + left.bounds.height / 2;
    const rightY = right.bounds.top + right.bounds.height / 2;
    return leftY - rightY;
  });
  const orderedSeeds = [];
  for (let row = 0; row < rows; ++row) {
    orderedSeeds.push(
      ...seeds
        .slice(row * columns, (row + 1) * columns)
        .sort((left, right) => left.bounds.left - right.bounds.left),
    );
  }

  const seedIndexByLabel = new Map(
    orderedSeeds.map((component, index) => [component.label, index]),
  );
  const componentOwner = new Map();
  for (const component of components) {
    if (seedIndexByLabel.has(component.label)) {
      componentOwner.set(component.label, seedIndexByLabel.get(component.label));
      continue;
    }
    if (component.size < 16) {
      continue;
    }
    let bestIndex = -1;
    let bestDistance = Number.POSITIVE_INFINITY;
    for (let index = 0; index < orderedSeeds.length; ++index) {
      const distance = boundsDistance(component.bounds, orderedSeeds[index].bounds);
      if (distance < bestDistance) {
        bestDistance = distance;
        bestIndex = index;
      }
    }
    if (bestDistance <= 96) {
      componentOwner.set(component.label, bestIndex);
    }
  }

  const buffers = Array.from({ length: expectedFrames }, () => Buffer.alloc(rgba.length));
  const counts = Array(expectedFrames).fill(0);
  for (let pixel = 0; pixel < width * height; ++pixel) {
    const label = labels[pixel];
    const owner = componentOwner.get(label);
    if (owner === undefined) {
      continue;
    }
    const offset = pixel * 4;
    rgba.copy(buffers[owner], offset, offset, offset + 4);
    ++counts[owner];
  }

  return buffers.map((data, index) => {
    const bounds = findAlphaBounds(data, width, height);
    if (!bounds || counts[index] < 1000) {
      fail(`Component frame ${index} is incomplete after grouping.`);
    }
    return { data, bounds, opaquePixels: counts[index] };
  });
}

function findAlphaBounds(rgba, width, height) {
  let minX = width;
  let minY = height;
  let maxX = -1;
  let maxY = -1;

  for (let y = 0; y < height; ++y) {
    for (let x = 0; x < width; ++x) {
      if (rgba[(y * width + x) * 4 + 3] <= 8) {
        continue;
      }
      minX = Math.min(minX, x);
      minY = Math.min(minY, y);
      maxX = Math.max(maxX, x);
      maxY = Math.max(maxY, y);
    }
  }

  if (maxX < minX || maxY < minY) {
    return null;
  }

  return {
    left: minX,
    top: minY,
    width: maxX - minX + 1,
    height: maxY - minY + 1,
  };
}

async function main() {
  const options = parseArguments(process.argv.slice(2));
  if (options.footY >= options.height) {
    fail("--foot-y must be less than --height.");
  }

  const source = await sharp(options.input).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
  const sourceWidth = source.info.width;
  const sourceHeight = source.info.height;
  await fs.mkdir(options.outputDirectory, { recursive: true });

  const manifest = {
    source: options.input,
    sourceSize: [sourceWidth, sourceHeight],
    layout: options.layout,
    grid: [options.columns, options.rows],
    outputSize: [options.width, options.height],
    footY: options.footY,
    key: options.key ?? "auto-border-median",
    frames: [],
  };
  const preparedFrames = [];

  if (options.layout === "components") {
    const frameKey = options.key ?? sampleBorderKey(source.data, sourceWidth, sourceHeight);
    const keyed = extractAlpha(source.data, frameKey);
    const componentFrames = buildComponentFrames(
      keyed.data,
      sourceWidth,
      sourceHeight,
      options.columns,
      options.rows,
    );
    for (let index = 0; index < componentFrames.length; ++index) {
      const componentFrame = componentFrames[index];
      preparedFrames.push({
        index,
        row: Math.floor(index / options.columns),
        keyedData: componentFrame.data,
        sourceCell: [0, 0, sourceWidth, sourceHeight],
        cellWidth: sourceWidth,
        cellHeight: sourceHeight,
        frameKey,
        bounds: componentFrame.bounds,
        opaquePixels: componentFrame.opaquePixels,
      });
    }
  } else {
    for (let row = 0; row < options.rows; ++row) {
      const top = Math.round((row * sourceHeight) / options.rows);
      const bottom = Math.round(((row + 1) * sourceHeight) / options.rows);
      for (let column = 0; column < options.columns; ++column) {
        const left = Math.round((column * sourceWidth) / options.columns);
        const right = Math.round(((column + 1) * sourceWidth) / options.columns);
        const cellWidth = right - left;
        const cellHeight = bottom - top;
        const cell = await sharp(source.data, {
          raw: { width: sourceWidth, height: sourceHeight, channels: 4 },
        })
          .extract({ left, top, width: cellWidth, height: cellHeight })
          .raw()
          .toBuffer();

        const frameKey = options.key ?? sampleBorderKey(cell, cellWidth, cellHeight);
        const keyed = extractAlpha(cell, frameKey);
        keyed.opaquePixels = removeSmallAlphaIslands(keyed.data, cellWidth, cellHeight);
        const bounds = findAlphaBounds(keyed.data, cellWidth, cellHeight);
        if (!bounds || keyed.opaquePixels < 100) {
          fail(`Frame ${preparedFrames.length} does not contain a usable subject.`);
        }

        preparedFrames.push({
          index: preparedFrames.length,
          row,
          keyedData: keyed.data,
          sourceCell: [left, top, cellWidth, cellHeight],
          cellWidth,
          cellHeight,
          frameKey,
          bounds,
          opaquePixels: keyed.opaquePixels,
        });
      }
    }
  }

  const horizontalLimit = options.width - 32;
  const commonScale = Math.min(
    options.targetHeight / Math.max(...preparedFrames.map((frame) => frame.bounds.height)),
    horizontalLimit / Math.max(...preparedFrames.map((frame) => frame.bounds.width)),
  );
  const rowBaselines = [];
  for (let row = 0; row < options.rows; ++row) {
    const bottoms = preparedFrames
      .filter((frame) => frame.row === row)
      .map((frame) => frame.bounds.top + frame.bounds.height)
      .sort((left, right) => left - right);
    rowBaselines.push(bottoms[Math.floor(bottoms.length / 2)]);
  }
  manifest.normalization = "common-scale-centered-x-row-baseline-y";
  manifest.commonScale = commonScale;
  manifest.rowBaselines = rowBaselines;

  for (const frame of preparedFrames) {
    const resizedWidth = Math.max(1, Math.round(frame.bounds.width * commonScale));
    const resizedHeight = Math.max(1, Math.round(frame.bounds.height * commonScale));
    const cropped = await sharp(frame.keyedData, {
      raw: { width: frame.cellWidth, height: frame.cellHeight, channels: 4 },
    })
      .extract(frame.bounds)
      .resize(resizedWidth, resizedHeight, { fit: "fill", kernel: sharp.kernel.lanczos3 })
      .png()
      .toBuffer();
    const outputLeft = Math.round((options.width - resizedWidth) / 2);
    const outputTop =
      options.footY -
      Math.round((rowBaselines[frame.row] - frame.bounds.top) * commonScale) +
      1;
    if (
      outputLeft < 0 ||
      outputTop < 0 ||
      outputLeft + resizedWidth > options.width ||
      outputTop + resizedHeight > options.height
    ) {
      fail(`Frame ${frame.index} cannot fit in the requested output canvas.`);
    }
    const outputName = `frame_${String(frame.index).padStart(2, "0")}.png`;
    await sharp({
      create: {
        width: options.width,
        height: options.height,
        channels: 4,
        background: { r: 0, g: 0, b: 0, alpha: 0 },
      },
    })
      .composite([{ input: cropped, left: outputLeft, top: outputTop }])
      .png({ compressionLevel: 9 })
      .toFile(path.join(options.outputDirectory, outputName));

    manifest.frames.push({
      index: frame.index,
      output: outputName,
      sourceCell: frame.sourceCell,
      sampledKey: frame.frameKey,
      alphaBounds: [
        frame.bounds.left,
        frame.bounds.top,
        frame.bounds.width,
        frame.bounds.height,
      ],
      outputBounds: [outputLeft, outputTop, resizedWidth, resizedHeight],
      opaquePixels: frame.opaquePixels,
    });
  }

  const previewWidth = options.columns * options.width;
  const previewHeight = options.rows * options.height;
  const checker = Buffer.alloc(previewWidth * previewHeight * 4);
  for (let y = 0; y < previewHeight; ++y) {
    for (let x = 0; x < previewWidth; ++x) {
      const shade = (Math.floor(x / 32) + Math.floor(y / 32)) % 2 === 0 ? 216 : 176;
      const offset = (y * previewWidth + x) * 4;
      checker[offset] = shade;
      checker[offset + 1] = shade;
      checker[offset + 2] = shade;
      checker[offset + 3] = 255;
    }
  }
  await sharp(checker, {
    raw: { width: previewWidth, height: previewHeight, channels: 4 },
  })
    .composite(
      manifest.frames.map((frame) => ({
        input: path.join(options.outputDirectory, frame.output),
        left: (frame.index % options.columns) * options.width,
        top: Math.floor(frame.index / options.columns) * options.height,
      })),
    )
    .png({ compressionLevel: 9 })
    .toFile(path.join(options.outputDirectory, "qa_contact_sheet.png"));

  await fs.writeFile(
    path.join(options.outputDirectory, "manifest.json"),
    `${JSON.stringify(manifest, null, 2)}\n`,
    "utf8",
  );
  console.log(
    `Prepared ${manifest.frames.length} frames at ${options.width}x${options.height}: ${options.outputDirectory}`,
  );
}

main().catch((error) => fail(error?.stack ?? String(error)));
