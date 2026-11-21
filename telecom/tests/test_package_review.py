import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

BASE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BASE / "tools"))
from package_review import ARCHIVE_ROOT, build, sha


class ReviewPackageTests(unittest.TestCase):
    def fixture(self, tmp):
        result = tmp / "results"
        result.mkdir()
        (result / "RESULTS_CN.md").write_text("# REVIEW_ONLY\n", encoding="utf-8")
        (result / "release_gate.json").write_text(
            json.dumps({"safe_to_build_vehicle_installer": False}), encoding="utf-8")
        return result

    def test_review_archive_contains_no_install_payload(self):
        with tempfile.TemporaryDirectory() as d:
            tmp = Path(d)
            r = self.fixture(tmp)
            out = tmp / "review.zip"
            meta = build(BASE, r, out)
            self.assertFalse(meta["installer_enabled"])
            self.assertFalse(meta["oem_binaries_included"])
            with zipfile.ZipFile(out) as z:
                names = z.namelist()
                self.assertEqual(len(names), 8)
                self.assertTrue(all(n.startswith(ARCHIVE_ROOT + "/") for n in names))
                self.assertFalse(any(
                    n.lower().endswith((".bin", ".so", ".img", ".elf", ".7z"))
                    for n in names
                ))
                disabled = z.read(ARCHIVE_ROOT + "/DO_NOT_INSTALL/INSTALL_DISABLED.sh")
                self.assertIn(b"exit 78", disabled)
                self.assertIn(b"NO_VERIFIED_CARRIER_SCOPED_FIX", disabled)
                m = z.read(ARCHIVE_ROOT + "/SHA256SUMS.txt").decode()
                for line in m.splitlines():
                    expected, path = line.split("  ", 1)
                    self.assertEqual(sha(z.read(ARCHIVE_ROOT + "/" + path)), expected)

    def test_reproducible_deterministic_zip(self):
        with tempfile.TemporaryDirectory() as d:
            tmp = Path(d)
            r = self.fixture(tmp)
            one, two = tmp / "1.zip", tmp / "2.zip"
            build(BASE, r, one)
            build(BASE, r, two)
            self.assertEqual(one.read_bytes(), two.read_bytes())

    def test_unblocked_gate_cannot_be_mislabeled_review_only(self):
        with tempfile.TemporaryDirectory() as d:
            tmp = Path(d)
            r = self.fixture(tmp)
            (r / "release_gate.json").write_text(
                json.dumps({"safe_to_build_vehicle_installer": True}),
                encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "CANNOT_PACKAGE_UNBLOCKED"):
                build(BASE, r, tmp / "review.zip")

    def test_missing_report_hard_fails(self):
        with tempfile.TemporaryDirectory() as d:
            tmp = Path(d)
            r = self.fixture(tmp)
            (r / "RESULTS_CN.md").unlink()
            with self.assertRaisesRegex(ValueError, "INPUT_NOT_REGULAR"):
                build(BASE, r, tmp / "review.zip")


if __name__ == "__main__":
    unittest.main()
