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

# End-to-end SD entry -> existing transaction fixture -> real temporary files.
import os
root=script.parents[2]
src=root/"tools/review_txn"
names=["btstack.fixture","dio.fixture","si.fixture","iap2.fixture"]
with tempfile.TemporaryDirectory(prefix="au57x-swdl-lab-",dir="/tmp") as carddir, tempfile.TemporaryDirectory(prefix="mu1005-c-txn-",dir="/tmp") as txdir:
    card=pathlib.Path(carddir); tx=pathlib.Path(txdir)
    (card/"LAB_ONLY").touch(); (tx/"LAB_ONLY").touch()
    pkg=card/"WirelessCarPlay"; pkg.mkdir()
    cluster=card/"ClusterMap"; cluster.mkdir()
    keep=cluster/"backup.txt"; keep.write_text("existing map")
    bin=card/"txn_fixture"
    subprocess.run(["cc","-std=c99","-Wall","-Wextra","-Werror",*[str(src/f) for f in ("txn.c","sha256.c","fixture_cli.c","supervisor_fixture.c")],"-o",str(bin)],check=True)
    for sub in ("target","backup","payload","state"): (tx/sub).mkdir()
    for i,n in enumerate(names):
        for d,prefix in (("target","original"),("payload","candidate")):
            f=tx/d/n; f.write_text(prefix+" "+n); f.chmod(0o755 if i<3 else 0o640)
    def go(code,marker,mode="ok"):
        env=dict(os.environ,TXN_LIFE_MODE=mode)
        env.pop("TXN_CHILD_EXIT",None); env.pop("TXN_CORRUPT_NEXT",None)
        r=subprocess.run(["sh",str(script),str(card),str(bin),str(tx)],env=env,capture_output=True,timeout=10)
        assert r.returncode==code,(r.returncode,r.stderr,(pkg/"update_result.txt").read_text())
        assert marker in (pkg/"update_result.txt").read_text()
        assert keep.read_text()=="existing map"
    def check(prefix):
        for n in names:
            assert (tx/"target"/n).read_text()==prefix+" "+n
            assert (tx/"backup"/n).read_text()=="original "+n
    (card/"Swdlautorun.txt.lab").touch()
    go(0,"LAB_INSTALL_COMMITTED")
    check("candidate")
    go(5,"LAB_TRANSACTION_BLOCKED") # repeat must not overwrite backup
    check("candidate")
    (pkg/"UNINSTALL.txt").touch()
    (tx/"payload").rename(tx/"removed-card")
    go(0,"LAB_RESTORE_COMPLETE")
    check("original")
    assert (tx/"state"/"journal").read_text().endswith("RESTORED\n")
print("PASS SD dispatcher to real fixture transaction, backup, duplicate refusal and restore without payload")
# Separate fresh transaction for failure and automatic rollback.
with tempfile.TemporaryDirectory(prefix="au57x-swdl-lab-",dir="/tmp") as carddir, tempfile.TemporaryDirectory(prefix="mu1005-c-txn-",dir="/tmp") as txdir:
    card=pathlib.Path(carddir); tx=pathlib.Path(txdir)
    (card/"LAB_ONLY").touch(); (tx/"LAB_ONLY").touch()
    pkg=card/"WirelessCarPlay"; pkg.mkdir()
    bin=card/"txn_fixture"
    subprocess.run(["cc","-std=c99","-Wall","-Wextra","-Werror",*[str(src/f) for f in ("txn.c","sha256.c","fixture_cli.c","supervisor_fixture.c")],"-o",str(bin)],check=True)
    for sub in ("target","backup","payload","state"): (tx/sub).mkdir()
    for i,n in enumerate(names):
        for d,prefix in (("target","original"),("payload","candidate")):
            f=tx/d/n; f.write_text(prefix+" "+n); f.chmod(0o755 if i<3 else 0o640)
    env=dict(os.environ,TXN_LIFE_MODE="bad-health")
    env.pop("TXN_CHILD_EXIT",None); env.pop("TXN_CORRUPT_NEXT",None)
    r=subprocess.run(["sh",str(script),str(card),str(bin),str(tx)],env=env,capture_output=True,timeout=10)
    assert r.returncode==4,(r.returncode,r.stderr)
    assert "LAB_INSTALL_ROLLED_BACK" in (pkg/"update_result.txt").read_text()
    assert (tx/"state"/"journal").read_text().endswith("RESTORED\n")
    for n in names: assert (tx/"target"/n).read_text()=="original "+n
print("PASS SD install health failure automatically restores original fixture files")
