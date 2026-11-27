#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import pathlib, subprocess, tempfile
script=pathlib.Path(__file__).resolve().with_name("update_run_lab.sh")
with tempfile.TemporaryDirectory(prefix="au57x-swdl-lab-",dir="/tmp") as tmp:
    card=pathlib.Path(tmp)
    pkg=card/"WirelessCarPlay"
    pkg.mkdir()
    cluster=card/"ClusterMap"
    cluster.mkdir()
    keep=cluster/"original-backup.txt"
    keep.write_text("preserve")
    def run(expected,mode=None):
        r=subprocess.run(["sh",str(script),str(card)],capture_output=True)
        assert r.returncode==expected,(r.returncode,r.stderr)
        if mode:
            result=(pkg/"update_result.txt").read_text()
            assert "Mode: "+mode in result and "NO INSTALL" in result
        assert keep.read_text()=="preserve"
    run(2)
    (card/"LAB_ONLY").touch()
    (card/"Swdlautorun.txt.lab").touch()
    run(3,"install")
    assert (card/"_Swdlautorun.txt.lab").exists()
    assert not (card/"Swdlautorun.txt.lab").exists()
    (pkg/"UNINSTALL.txt.txt").touch()
    run(3,"uninstall")
    assert "Mode: install" in (pkg/"update_result.1.txt").read_text()
    (pkg/"CoLlEcT.txt").touch()
    run(0,"collect")
    assert "Mode: uninstall" in (pkg/"update_result.2.txt").read_text()
    (pkg/"CoLlEcT.txt").unlink()
    (pkg/"collect_logs.sh").touch()
    run(3,"uninstall")
    (pkg/"UNINSTALL.txt.txt").unlink()
    run(3,"install")
    (pkg/"update_result.txt").unlink()
    (pkg/"update_result.txt").symlink_to(keep)
    run(2)
print("PASS SWDL lab dispatch, collect priority, log rotation, one-shot marker, ClusterMap isolation")
