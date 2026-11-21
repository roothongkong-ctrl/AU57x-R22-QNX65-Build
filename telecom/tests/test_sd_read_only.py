import hashlib
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

BASE = Path(__file__).resolve().parents[1]
PROBE = BASE / "sd" / "READ_ONLY_PROBE.sh"


class SDReadOnlyTests(unittest.TestCase):
    def setup_fixture(self, root):
        app = root / "app"
        sd = root / "sd"
        sd.mkdir()
        items = (
            ("eso/bin/apps/nad", 845158),
            ("eso/bin/apps/telephone", 1458120),
            ("eso/bin/apps/connectionmanager", 758371),
            ("eso/telephone/autoconf.xml", 92014),
        )
        for path, n in items:
            f = app / path
            f.parent.mkdir(parents=True, exist_ok=True)
            f.write_bytes(b"X" * n)
        return app, sd, [app / x for x, _ in items]

    def run_probe(self, app, sd, root):
        script = PROBE.read_text(encoding="utf-8")
        self.assertIn("APP=/mnt/app", script)
        self.assertIn("SD=/net/mmx/fs/sda0", script)
        script = script.replace("APP=/mnt/app", "APP=" + str(app))
        script = script.replace("SD=/net/mmx/fs/sda0", "SD=" + str(sd))
        out = root / "probe_mock.sh"
        out.write_text(script, encoding="utf-8")
        return subprocess.run(["sh", str(out)], capture_output=True,
                              text=True, timeout=10)

    def test_all_four_files_size_match_without_writes(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            app, sd, originals = self.setup_fixture(root)
            before = [hashlib.sha256(p.read_bytes()).hexdigest() for p in originals]
            proc = self.run_probe(app, sd, root)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            report = (sd / "au57x_ct_r03_readonly" / "RESULT_CN.txt").read_text()
            self.assertIn("NAD=SIZE_MATCH_ONLY:845158", report)
            self.assertIn("TELEPHONE=SIZE_MATCH_ONLY:1458120", report)
            self.assertIn("CONNECTIONMANAGER=SIZE_MATCH_ONLY:758371", report)
            self.assertIn("AUTOCONF=SIZE_MATCH_ONLY:92014", report)
            self.assertIn("LIVE_LTE_REGISTRATION=NOT_MEASURED", report)
            self.assertIn("TARGET_MUTATIONS=NONE", report)
            after = [hashlib.sha256(p.read_bytes()).hexdigest() for p in originals]
            self.assertEqual(before, after)

    def test_symlink_output_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            app, sd, originals = self.setup_fixture(root)
            outside = root / "outside"
            outside.mkdir()
            (sd / "au57x_ct_r03_readonly").symlink_to(outside, target_is_directory=True)
            proc = self.run_probe(app, sd, root)
            self.assertNotEqual(proc.returncode, 0)
            self.assertIn("STOP=SYMLINK_OUTPUT_REJECTED", proc.stdout)
            self.assertEqual(list(outside.iterdir()), [])

    def test_size_mismatch_never_claims_original_elf_identity(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            app, sd, originals = self.setup_fixture(root)
            originals[0].write_bytes(b"short")
            proc = self.run_probe(app, sd, root)
            self.assertEqual(proc.returncode, 0)
            report = (sd / "au57x_ct_r03_readonly" / "RESULT_CN.txt").read_text()
            self.assertIn("NAD=SIZE_MISMATCH:5", report)
            self.assertNotIn("NAD=SIZE_MATCH_ONLY", report)
            self.assertIn("SOURCE_BINARY_HASH=NOT_COMPUTED_ON_HEADUNIT", report)

    def test_static_no_network_command_or_app_write(self):
        script = PROBE.read_text()
        for banned in ("mount -uw", "AT+COPS=", "AT^", "/dev/NAD/AT_port",
                       "chmod ", "chown ", "flash", "reboot",
                       "rm -f", "mv -f", "dd if="):
            with self.subTest(banned=banned):
                self.assertNotIn(banned, script)


if __name__ == "__main__":
    unittest.main()
