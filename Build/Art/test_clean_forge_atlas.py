import unittest

import numpy as np

from clean_forge_atlas import background_mask


class ConnectedBackgroundTests(unittest.TestCase):
    def test_gray_inside_dark_outline_is_preserved(self):
        rgba = np.full((9, 9, 4), 220, dtype=np.uint8)
        rgba[:, :, 3] = 255
        rgba[2:7, 2:7, :3] = 20
        rgba[3:6, 3:6, :3] = 240
        mask = background_mask(rgba, [(0, 0)])
        self.assertTrue(mask[0, 0])
        self.assertFalse(mask[2, 2])
        self.assertFalse(mask[4, 4])

    def test_explicit_hole_seed_only_removes_enclosed_gray(self):
        rgba = np.full((9, 9, 4), 220, dtype=np.uint8)
        rgba[:, :, 3] = 255
        rgba[2:7, 2:7, :3] = 20
        rgba[3:6, 3:6, :3] = 240
        mask = background_mask(rgba, [(0, 0), (4, 4)])
        self.assertTrue(mask[4, 4])
        self.assertFalse(mask[2, 2])

    def test_colored_or_invalid_seed_rejected(self):
        rgba = np.full((4, 4, 4), 255, dtype=np.uint8)
        rgba[1, 1, :3] = (250, 140, 140)
        for seed in ((1, 1), (-1, 0), (4, 0)):
            with self.subTest(seed=seed), self.assertRaises(ValueError):
                background_mask(rgba, [seed])


if __name__ == "__main__":
    unittest.main()
