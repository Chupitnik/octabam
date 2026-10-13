"""tools/hw/bcr2000.py page2: CC MAP's block CCs through the settings layers."""
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "hw"))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))
import bcr2000  # noqa: E402
from remix import brainfile  # noqa: E402


class Page2(unittest.TestCase):
    def test_defaults(self):
        self.assertEqual(bcr2000.page2(), {"FX2": 62, "FX1": 68})

    def test_project_layer(self):
        rec = brainfile.Record(brainfile.SETTINGS, "octabam.cc-map",
                               payload=brainfile.Value(2, brainfile.NUMBER, (80).to_bytes(2, "big")).encode())
        with tempfile.TemporaryDirectory() as d:
            (pathlib.Path(d) / "brain.work").write_bytes(brainfile.File(brainfile.PROJECT_WORK, [rec]).encode())
            self.assertEqual(bcr2000.page2("octabam-bus", d), {"FX2": 62, "FX1": 80})

    def test_damaged_work_falls_back_to_strd(self):
        rec = brainfile.Record(brainfile.SETTINGS, "octabam.cc-map",
                               payload=brainfile.Value(1, brainfile.NUMBER, (90).to_bytes(2, "big")).encode())
        with tempfile.TemporaryDirectory() as d:
            (pathlib.Path(d) / "brain.work").write_bytes(b"BRAN damaged")
            (pathlib.Path(d) / "brain.strd").write_bytes(brainfile.File(brainfile.PROJECT_STRD, [rec]).encode())
            self.assertEqual(bcr2000.page2(None, d), {"FX2": 90, "FX1": 68})


if __name__ == "__main__":
    unittest.main()
