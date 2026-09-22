"""Read-only regression checks against the original ascension PNG and C++ crop table."""
from pathlib import Path
import re
import unittest

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]


class AscensionPoseLayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pixels = np.asarray(Image.open(ROOT / "Content/GAME/Asset/Player/player_ascension.png").convert("RGBA"))
        header = (ROOT / "Source/ImmortalPath/UI/ImmortalAscensionSequenceLayout.h").read_text(encoding="utf-8")
        table = re.search(r"FrameLeftPixels\[\]\s*=\s*\{([^}]+)\}", header).group(1)
        cls.lefts = [int(value) for value in re.findall(r"\d+", table)]
        columns = (cls.pixels[:, :, 3] > 0).any(axis=0)
        transitions = np.diff(np.pad(columns.astype(int), (1, 1)))
        cls.bounds = list(zip(np.flatnonzero(transitions == 1), np.flatnonzero(transitions == -1)))

    def test_original_sheet_has_sixteen_separate_poses(self):
        self.assertEqual(self.pixels.shape, (724, 2176, 4))
        self.assertEqual(len(self.bounds), 16)
        self.assertEqual(len(self.lefts), 16)

    def test_each_window_contains_one_complete_pose_and_no_neighbor(self):
        for left, (start, end) in zip(self.lefts, self.bounds):
            with self.subTest(left=left):
                self.assertLessEqual(left, start)
                self.assertGreaterEqual(left + 128, end)
                full = self.pixels[:, start:end, 3]
                crop = self.pixels[190:620, left:left + 128, 3]
                self.assertEqual(int(crop.sum()), int(full.sum()))
                self.assertEqual(np.count_nonzero(crop), np.count_nonzero(full))


if __name__ == "__main__":
    unittest.main()
