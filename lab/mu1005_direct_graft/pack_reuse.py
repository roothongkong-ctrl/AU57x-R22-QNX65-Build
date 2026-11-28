#!/usr/bin/env python3
"""Reuse the compiled original-AP donor and existing recovery code in one review bundle.
Not an installer: the direct graft and supervised transport are not connected.
"""
import hashlib,json,sys,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
prior,report,out=map(Path,sys.argv[1:])
def sha(b):return hashlib.sha256(b).hexdigest()
r=json.loads(report.read_text())
assert r["installable"] is False and r["remote_accept"] is False
binname="donor-daemon-MU1005-INHERITED-ACK-NO-INSTALL.bin"
binary=(prior/binname).read_bytes()
expected=(prior/"daemon.sha256").read_text().split()[0]
assert len(expected)==64 and sha(binary)==expected and binary[:6]==b"\x7fELF\x01\x01"
assert int.from_bytes(binary[18:20],"little")==40
status=(prior/"STATUS.txt").read_text()
assert "source=de5d7643645c3d486a258cc10758ae6ca4cbb9a7" in status
assert "NO INSTALL" in status
payload={"daemon/"+binname:binary,"daemon/STATUS.txt":status.encode(),
         "evidence/direct-graft.json":report.read_bytes()}
for p in ("LICENSE","LICENSE.md","COPYING"):
    if (prior/p).is_file():payload["daemon/"+p]=(prior/p).read_bytes()
for p in ("combined-port.diff","daemon.sha256"):
    payload["daemon/"+p]=(prior/p).read_bytes()
paths=[
 "lab/mu1005_direct_graft/build.py",
 "tools/mu1005_lifecycle_observer.py",
 "integration/mu1005_vehicle_stage/congo_port_table.json",
 "project/source/legacy_mu1005/bluetooth/native_event.c",
 "project/source/legacy_mu1005/bluetooth/native_binding_words.h",
 "integration/mu1005_vehicle_stage/backup.sh",
 "integration/mu1005_vehicle_stage/recover.sh",
 "integration/mu1005_vehicle_stage/config_transaction.sh",
 "integration/mu1005_vehicle_stage/stock_config_apply.sh",
 "lab/mu1005_route4_5/daemon_supervisor.c",
 "lab/mu1005_route4_5/daemon_supervisor.h",
 "lab/mu1005_route4_5/supervised_notify.c",
 "lab/mu1005_route4_5/supervised_notify.h",
 "lab/mu1005_route4_5/supervised_startup.c",
 "lab/mu1005_route4_5/supervised_startup.h",
 "integration/mu1005_donor_daemon/README_CN.md",
]
for p in paths:payload["source-review/"+p]=(root/p).read_bytes()
payload["README_CN.txt"]=("MU1005 无线 CarPlay 集中整合复核包，不可装车。\n"
"复用 R105.249 已编译的完整 QNX ARM 守护程序（原厂热点、自动 IE 槽位、ACK、继承通信端点），不重复编译。\n"
"原厂蓝牙启动模块的最新构建报告、已有安全回调及通信监督源码、备份与恢复脚本一并归档。\n"
"蓝牙与守护程序之间的真实运行时接线、停止及有线切换仍未完成；恢复脚本仅备份/暂存，不是自动恢复安装器。\n"
"禁止将本包放入自动安装目录。无需再次提交车机检测包。\n").encode()
manifest={"scope":"CONSOLIDATED_REUSE_REVIEW_ONLY","installable":False,"vehicle_verified":False,
 "reused_daemon_run":46109600160,"reused_daemon_artifact":15248733614,
 "remaining":["actual supervised transport ownership and wire ACK","native lifecycle shutdown and wired handoff","automatic installation and restoration","actual iPhone connection"],
 "files":[{"path":n,"sha256":sha(b),"bytes":len(b)} for n,b in sorted(payload.items())]}
payload["manifest.json"]=(json.dumps(manifest,ensure_ascii=False,indent=2)+"\n").encode()
with zipfile.ZipFile(out,"x",zipfile.ZIP_DEFLATED) as z:
    for n,b in sorted(payload.items()):z.writestr(n,b)
print("MU1005_REUSED_COMPONENTS_CONSOLIDATED_NO_INSTALL")
