import os,unittest
from pathlib import Path
from fallback_lab import run_lab,ROUTES
BIN=Path(os.environ["TXN_TEST_BIN"]).resolve()
class Fallback(unittest.TestCase):
    def test_first_works(self):
        self.assertEqual(run_lab(BIN,["ok"]*4),("SELECTED",[(ROUTES[0],"FIXTURE_SELECTED")]))
    def test_third_after_two_actual_restores(self):
        self.assertEqual(run_lab(BIN,["bad-health","timeout","ok","ok"]),("SELECTED",[(ROUTES[0],"RESTORED"),(ROUTES[1],"RESTORED"),(ROUTES[2],"FIXTURE_SELECTED")]))
    def test_four_fail(self):
        self.assertEqual(run_lab(BIN,["start-fails","bad-health","timeout","bad-health"]),("ALL_RESTORED",[(r,"RESTORED") for r in ROUTES]))
    def test_restore_blocked(self):
        self.assertEqual(run_lab(BIN,["bad-health","stop-candidate-fails","ok","ok"]),("BLOCKED",[(ROUTES[0],"RESTORED"),(ROUTES[1],"BLOCKED")]))
    def test_crash_blocks_next(self):
        self.assertEqual(run_lab(BIN,["crash-after-start","ok","ok","ok"]),("BLOCKED",[(ROUTES[0],"BLOCKED")]))
    def test_original_stop_fails(self):
        self.assertEqual(run_lab(BIN,["stop-original-fails","ok","ok","ok"]),("BLOCKED",[(ROUTES[0],"BLOCKED")]))
if __name__=="__main__":unittest.main(verbosity=2)
