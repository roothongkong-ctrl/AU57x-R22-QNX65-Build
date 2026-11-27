"""Enable candidate reply lifetime on Linux in a disposable checkout only."""
from pathlib import Path
import sys
mode=sys.argv[1]
if mode not in ("hold","immediate"):raise ValueError("invalid mode")
p=Path("project/source/legacy_mu1005/airplay/cprx_service.c")
s=p.read_text()
anchor="#if defined(__QNXNTO__) && !defined(MU_CPRX_QNX_IMMEDIATE_CLOSE)"
if s.count(anchor)!=2:raise ValueError("unexpected source version")
if mode=="hold":
    p.write_text(s.replace(anchor,"#if 1 /* TEST ONLY: exercise QNX ACK lifetime on Linux */"))
p=Path("ci/host_cloud_pipeline.sh")
s=p.read_text()
anchor="-DMU_AP_REFRESH_TEST -rdynamic"
if s.count(anchor)!=1:raise ValueError("unexpected compile flags")
p.write_text(s.replace(anchor,"-DMU_AP_REFRESH_TEST -DMU1005_QNX_FIRST_LINK_FIXTURE -Wno-unused-function -rdynamic"))
