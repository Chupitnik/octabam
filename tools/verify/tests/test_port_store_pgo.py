"""port_store: install prefers the store's PGO build, OCTABAM_PGO=0 asks for the plain one,
the PGO directory follows pgo.sh's bytes, and the profile is not installed."""
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools/emu"))
sys.path.insert(0, str(ROOT / "tools/verify"))
import port_store  # noqa: E402
import reach  # noqa: E402


class Pgo(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.tmp.name)
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        (self.root / "tools/emu/ot_emu").mkdir(parents=True)
        (self.root / "tools/emu/ot_emu/pgo.sh").write_text("#!/bin/bash\necho one\n")
        self.real_key, self.real_stamp = port_store.key, reach.stamp_port
        port_store.key = lambda root=self.root: "K" * 64
        reach.stamp_port = lambda *a, **k: None

    def tearDown(self):
        port_store.key, reach.stamp_port = self.real_key, self.real_stamp
        self.tmp.cleanup()

    def fill(self, d, text):
        d.mkdir(parents=True)
        (d / "ot_emu").write_text(text)
        (d / "ot_dsp_test").write_text(text)
        (d / "ot_emu.profdata").write_text("profile")
        (d / "pgo.log").write_text("log")

    def test_the_pgo_directory_follows_pgo_sh(self):
        a = port_store.pgo_dir("K" * 64, self.root)
        (self.root / "tools/emu/ot_emu/pgo.sh").write_text("#!/bin/bash\necho two\n")
        self.assertNotEqual(a, port_store.pgo_dir("K" * 64, self.root))
        self.assertTrue(a.name.startswith("K" * 64 + "-pgo-"))

    def test_install_takes_the_pgo_build_and_only_the_executables(self):
        self.fill(port_store.built.__globals__["store_dir"](self.root) / ("K" * 64), "plain")
        self.fill(port_store.pgo_dir("K" * 64, self.root), "pgo")
        dest, d = port_store.install(self.root / "out/emu", self.root)
        self.assertEqual((dest / "ot_emu").read_text(), "pgo")
        self.assertEqual((dest / "ot_dsp_test").read_text(), "pgo")
        self.assertEqual(sorted(p.name for p in dest.iterdir()), ["ot_dsp_test", "ot_emu"])    # no profdata, no log
        self.assertIn("-pgo-", d.name)

    def test_octabam_pgo_0_asks_for_the_plain_build(self):
        self.fill(port_store.built.__globals__["store_dir"](self.root) / ("K" * 64), "plain")
        self.fill(port_store.pgo_dir("K" * 64, self.root), "pgo")
        os.environ["OCTABAM_PGO"] = "0"
        try:
            dest, d = port_store.install(self.root / "out/emu", self.root)
        finally:
            del os.environ["OCTABAM_PGO"]
        self.assertEqual((dest / "ot_emu").read_text(), "plain")
        self.assertNotIn("-pgo-", d.name)


if __name__ == "__main__":
    unittest.main()
