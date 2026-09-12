"""Remove only manually seeded connected gray backdrop from the 41O forge atlas.

Not a generic chroma key: dark object outlines block the flood. Coordinates and
dimensions are tied to the reviewed source; preserve that original alongside output.
"""
import argparse
from collections import deque
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image


def background_mask(rgba, seeds):
    rgb = rgba[:, :, :3].astype(np.int16)
    eligible = (rgb.min(axis=2) >= 110) & ((rgb.max(axis=2) - rgb.min(axis=2)) <= 38)
    height, width = eligible.shape
    removed = np.zeros((height, width), dtype=bool)
    queue = deque()
    for x, y in seeds:
        if not (0 <= x < width and 0 <= y < height and eligible[y, x]):
            raise ValueError(f"Reviewed background seed no longer valid: {x},{y}")
        if not removed[y, x]:
            removed[y, x] = True
            queue.append((x, y))
    while queue:
        x, y = queue.popleft()
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < width and 0 <= ny < height and eligible[ny, nx] and not removed[ny, nx]:
                removed[ny, nx] = True
                queue.append((nx, ny))
    return removed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    if args.source.resolve() == args.output.resolve() or args.output.exists():
        raise ValueError("Use a new output path; never overwrite the source or an existing version")
    if args.preview and (args.preview.exists() or args.preview.resolve() in
                         (args.source.resolve(), args.output.resolve())):
        raise ValueError("Preview must also use a distinct new path")
    reviewed_source = "e4816a895f7dadd9db9f64f5716212f75bfd73f839276ebe183375399699ebf8"
    if hashlib.sha256(args.source.read_bytes()).hexdigest() != reviewed_source:
        raise ValueError("This preset only supports the exact manually reviewed forge source")
    with Image.open(args.source) as image:
        rgba = np.array(image.convert("RGBA"))
    if rgba.shape != (1024, 1536, 4):
        raise ValueError("Only the reviewed 1536x1024 forge atlas is supported")
    # Outer backdrop plus the two enclosed furnace handle holes, visually reviewed.
    seeds = [(0, 0), (512, 512), (1024, 512), (1095, 808), (1446, 808)]
    removed = background_mask(rgba, seeds)
    output = rgba.copy()
    output[removed, 3] = 0
    # Guarantee no retained object pixel, RGB value, or canvas dimension changed.
    assert np.array_equal(output[~removed], rgba[~removed])
    assert np.array_equal(output[:, :, :3], rgba[:, :, :3])
    assert not output[0, :, 3].any() and not output[-1, :, 3].any()
    assert not output[:, 0, 3].any() and not output[:, -1, 3].any()
    bounds = []
    for row in range(2):
        for col in range(3):
            cell = output[row * 512:(row + 1) * 512, col * 512:(col + 1) * 512, 3]
            ys, xs = np.nonzero(cell)
            assert xs.size
            bounds.append([int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    result = Image.fromarray(output)
    result.save(args.output)
    if args.preview:
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        preview = Image.new("RGBA", result.size, (35, 55, 54, 255))
        preview.alpha_composite(result)
        preview.convert("RGB").save(args.preview)
    print(json.dumps({"removed_pixels": int(removed.sum()), "retained_pixels": int((~removed).sum()),
                      "cell_bounds": bounds, "output": str(args.output)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
