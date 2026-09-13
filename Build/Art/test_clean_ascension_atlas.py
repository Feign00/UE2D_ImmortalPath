import unittest
from pathlib import Path

import numpy as np
from PIL import Image
from clean_ascension_atlas import clean, SEEDS


class AscensionAtlasTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = Path(__file__).resolve().parents[2]
        folder = root / "ArtSource/MortalRealm/DesktopPixelV2/Ascension"
        cls.original = np.array(Image.open(folder / "ascension-atlas-source-v1.png").convert("RGBA"))
        image, cls.report = clean(folder / "ascension-atlas-source-v1.png")
        cls.cleaned = np.array(image)
        cls.final = np.array(Image.open(folder / "ascension-atlas-v2.png"))

    def test_final_is_reproducible_and_preserves_rgb(self):
        np.testing.assert_array_equal(self.cleaned, self.final)
        np.testing.assert_array_equal(self.cleaned[:, :, :3], self.original[:, :, :3])
        self.assertTrue((self.cleaned[self.cleaned[:, :, 3] > 0, 3] == 255).all())

    def test_holes_clear_while_portal_and_pearl_remain(self):
        for x, y in SEEDS:
            self.assertEqual(self.cleaned[y, x, 3], 0)
        for x, y in ((765, 675), (1280, 737), (272, 440), (257, 827)):
            self.assertEqual(self.cleaned[y, x, 3], 255)

    def test_six_populated_cells_have_transparent_edges(self):
        self.assertEqual(len(self.report["cell_bounds"]), 6)
        for index in range(6):
            x, y = index % 3 * 512, index // 3 * 512
            alpha = self.cleaned[y:y+512, x:x+512, 3]
            self.assertFalse(alpha[0].any() or alpha[-1].any() or alpha[:, 0].any() or alpha[:, -1].any())
            self.assertGreater(np.count_nonzero(alpha), 10000)


if __name__ == "__main__":
    unittest.main()
