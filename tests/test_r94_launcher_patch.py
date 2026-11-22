import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import r94_launcher_patch as patch


class LauncherTests(unittest.TestCase):
    def test_list_layout_preserves_all_context_and_factory_gate(self):
        src = {"other": {"kept": True}, "modules": [
            {"name": "bluetooth", "exec": "/eso/bin/apps/bluetooth"},
            {"name": "btstack", "exec": patch.STOCK, "preCondition": "/tmp/mvloaded", "restart": 2, "args": ["a"]}]}
        raw = json.dumps(src).encode()
        with tempfile.TemporaryDirectory() as d:
            out = Path(d) / "review"
            m = patch.stage(raw, out, expected_sha=hashlib.sha256(raw).hexdigest())
            self.assertFalse(m["vehicle_verified"])
            self.assertFalse(m["installable"])
            self.assertEqual((out/"connectivity.original.json").read_bytes(), raw)
            after = json.loads((out/"connectivity.STAGED_REVIEW_ONLY.json").read_text())
            self.assertEqual(after["modules"][0], src["modules"][0])
            self.assertEqual(after["modules"][1]["preCondition"], "/tmp/mvloaded")
            self.assertEqual(after["modules"][1]["restart"], 2)
            self.assertEqual(after["modules"][1]["args"], ["a"])
            script = (out / "au57x_btstack_probe_entry.sh").read_text()
            self.assertIn("exec /eso/bin/apps/btstack \"$@\"", script)
            self.assertNotIn("kill", script)
            with self.assertRaises(ValueError):
                patch.stage(raw, out)

    def test_dict_layout(self):
        for src in ({"btstack": {"exec": patch.STOCK}},
                    {"processes": {"btstack": {"exec": patch.STOCK}}},
                    {"jobs": [{"id": "btstack", "exec": patch.STOCK}]}):
            with tempfile.TemporaryDirectory() as d:
                patch.stage(json.dumps(src).encode(), Path(d)/"out")

    def test_reject_unknown_ambiguous_or_nonstock(self):
        cases = [{"modules": []},
                 {"modules": [{"name": "btstack", "exec": "bad"}]},
                 {"btstack": {"exec": patch.STOCK}, "modules": [{"name": "btstack", "exec": patch.STOCK}]},
                 {"modules": [{"name": "btstack", "exec": patch.STOCK}, {"name": "btstack", "exec": patch.STOCK}]},
                 {"modules": {"bluetooth": {"exec": patch.STOCK}}}]
        for case in cases:
            with tempfile.TemporaryDirectory() as d:
                with self.assertRaises(ValueError):
                    patch.stage(json.dumps(case).encode(), Path(d)/"out")

    def test_reject_tampering_and_invalid_path(self):
        raw = json.dumps({"btstack": {"exec": patch.STOCK}}).encode()
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(ValueError, "mismatch"):
                patch.stage(raw, Path(d)/"out", expected_sha="0"*64)
            with self.assertRaisesRegex(ValueError, "absolute"):
                patch.stage(raw, Path(d)/"out", wrapper="relative")
            with self.assertRaisesRegex(ValueError, "quoting"):
                patch.stage(raw, Path(d)/"out", wrapper="/bad'quote")
            self.assertFalse((Path(d)/"out").exists())

    def test_reject_non_json(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(ValueError):
                patch.stage(b"bad", Path(d)/"out")


if __name__ == "__main__":
    unittest.main()
