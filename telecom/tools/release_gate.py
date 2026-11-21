#!/usr/bin/env python3
"""AU57x telecom safe-release gate; review-only unless every requirement proves out.

This evaluates supplied facts. It does not establish facts by assertion and
must be paired with binary/script tests and human approval.
"""
import argparse
import json
from pathlib import Path

INSTALL_REQUIREMENTS = (
    "real_oem_identity_verified",
    "upstream_network_mode_source_identified",
    "telecom_mode_failure_path_verified",
    "targeted_carrier_fix_implemented",
    "china_mobile_original_path_preserved",
    "arm_qnx_abi_verified",
    "restore_is_independently_verified",
    "vehicle_original_backup_exists",
    "failure_injection_rollback_verified",
    "reviewed_release_integrity",
)
LTE_SUCCESS_REQUIREMENTS = INSTALL_REQUIREMENTS + (
    "live_telecom_lte_registration_observed",
    "live_telecom_data_session_observed",
    "live_china_mobile_regression_passed",
)

def evaluate(verified_evidence):
    if not isinstance(verified_evidence, dict):
        raise TypeError("expected evidence mapping")
    # Only strict literal boolean True counts; non-empty strings never pass.
    install_missing = [
        k for k in INSTALL_REQUIREMENTS
        if verified_evidence.get(k) is not True
    ]
    success_missing = [
        k for k in LTE_SUCCESS_REQUIREMENTS
        if verified_evidence.get(k) is not True
    ]
    return {
        "safe_to_build_vehicle_installer": not install_missing,
        "safe_to_claim_telecom_lte_works": not success_missing,
        "install_blockers": install_missing,
        "lte_claim_blockers": success_missing,
        "policy": "NO_INSTALL_BY_DEFAULT; GitHub simulation does not prove live LTE.",
    }

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--evidence", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    r = evaluate(json.loads(a.evidence.read_text(encoding="utf-8")))
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(r, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("INSTALL_GATE:", "OPEN" if r["safe_to_build_vehicle_installer"] else "BLOCKED")
    print("LTE_LIVE_GATE:", "OPEN" if r["safe_to_claim_telecom_lte_works"] else "BLOCKED")
    # A correctly BLOCKED result is success for a review pipeline.
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
