"""Clean reviewed 41Q ascension checkerboard; retain original pixels and layout."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image
from clean_forge_atlas import background_mask

SOURCE_SHA256 = "f099bb290d26e22bb831c6af62858b5b5abb0320ed17bfdde1deaa0f71d8c3fe"
# Background, coin hole, rebirth loop interior and lower ribbon opening.
# Coordinates are valid only for the exact visually reviewed source above.
SEEDS = [(0, 0), (512, 512), (1024, 512), (1247, 284), (1270, 665), (1274, 892)]


def clean(source):
    if hashlib.sha256(source.read_bytes()).hexdigest() != SOURCE_SHA256:
        raise ValueError("Only the exact reviewed source is supported")
    with Image.open(source) as image:
        rgba = np.array(image.convert("RGBA"))
    if rgba.shape != (1024, 1536, 4):
        raise ValueError("Expected a 1536x1024 atlas")
    removed = background_mask(rgba, SEEDS)
    # Six isolated neutral background specks, verified by connected-component
    # bounds and source inspection. Do not remove all small components: flames
    # and scroll decorations are intentionally detached.
    for x, y in ((911, 190), (1150, 531), (1150, 533), (1151, 533),
                 (1528, 597), (1412, 930)):
        assert int(rgba[y, x, :3].max()) - int(rgba[y, x, :3].min()) <= 10
        removed[y, x] = True
    output = rgba.copy()
    output[removed, 3] = 0
    assert np.array_equal(output[:, :, :3], rgba[:, :, :3])
    assert np.array_equal(output[~removed], rgba[~removed])
    bounds = []
    for row in range(2):
        for col in range(3):
            alpha = output[row*512:(row+1)*512, col*512:(col+1)*512, 3]
            assert not alpha[0].any() and not alpha[-1].any()
            assert not alpha[:, 0].any() and not alpha[:, -1].any()
            ys, xs = np.nonzero(alpha)
            assert xs.size > 1000
            bounds.append([int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())])
    return Image.fromarray(output), {"removed_pixels": int(removed.sum()), "cell_bounds": bounds}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    paths = [args.source.resolve(), args.output.resolve()]
    if args.preview:
        paths.append(args.preview.resolve())
    if len(set(paths)) != len(paths) or args.output.exists() or (args.preview and args.preview.exists()):
        raise ValueError("Use distinct new output paths; never overwrite source or existing files")
    image, report = clean(args.source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.output)
    if args.preview:
        preview = Image.new("RGBA", (768, 256), (26, 42, 38, 255))
        for index in range(6):
            x, y = index % 3 * 512, index // 3 * 512
            icon = image.crop((x, y, x + 512, y + 512))
            preview.alpha_composite(icon.resize((128, 128), Image.Resampling.LANCZOS), (index * 128, 0))
            preview.alpha_composite(icon.resize((72, 72), Image.Resampling.LANCZOS), (index * 128 + 28, 152))
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        preview.convert("RGB").save(args.preview)
    print(json.dumps(report))


if __name__ == "__main__":
    main()
