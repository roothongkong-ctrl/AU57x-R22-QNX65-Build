#!/usr/bin/env python3
"""Offline ONLY: generate a reversible OEM launcher config *review* staging set.
Never edit an OEM file, write to a car, or start stock btstack.
The exact AU57x connectivity.json schema is not yet pinned; reject unknown forms.
"""
import argparse
import hashlib
import json
import os
import pathlib
import shlex
import shutil
import sys
import tempfile

STOCK = "/eso/bin/apps/btstack"
WRAPPER = "/mnt/app/eso/bin/apps/au57x_btstack_probe_entry.sh"
PRELOAD = "/mnt/app/eso/lib/libau57x_bt_preload_qnx_REVIEW_ONLY.so"
STOCK_SHA = "51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def target_nodes(config):
    # Only explicitly supported unambiguous shapes. Never alter a different process.
    found = []
    if isinstance(config, dict):
        node = config.get("btstack")
        if isinstance(node, dict):
            found.append(node)
        for key in ("modules", "processes", "applications", "jobs"):
            group = config.get(key)
            if isinstance(group, list):
                found.extend(item for item in group if isinstance(item, dict) and
                             (item.get("name") == "btstack" or item.get("id") == "btstack"))
            elif isinstance(group, dict):
                candidate = group.get("btstack")
                if isinstance(candidate, dict):
                    found.append(candidate)
    if len(found) != 1:
        raise ValueError(f"expected exactly one btstack module, found {len(found)}; unknown OEM schema denied")
    node = found[0]
    if node.get("exec") != STOCK:
        raise ValueError("stock executable not exact: refusing ambiguous/modified launcher")
    # No 'args'/parameter rewriting: supervisor may have hidden custom process semantics.
    return node


def stage(content, out_dir, wrapper=WRAPPER, preload=PRELOAD, expected_sha=None, hfp_probe=False):
    if expected_sha is not None and digest(content) != expected_sha:
        raise ValueError("original launcher configuration SHA256 mismatch")
    if not wrapper.startswith("/") or not preload.startswith("/") or "\n" in wrapper + preload:
        raise ValueError("absolute, newline-free QNX paths required")
    for path in (wrapper, preload):
        if "'" in path or '"' in path or "\\" in path or "\r" in path:
            raise ValueError("path quoting/escaping not supported")
    config = json.loads(content)
    node = target_nodes(config)
    old_precondition = node.get("preCondition", None)
    node["exec"] = wrapper
    # Verify original supervisor gate/other keys are left unmodified.
    assert node.get("preCondition", None) == old_precondition
    modified = (json.dumps(config, indent=2, ensure_ascii=False) + "\n").encode()
    if type(hfp_probe) is not bool:
        raise ValueError("R97 HFP probe mode must be an explicit boolean")
    # A self-contained wrapper replaces the *same* process with exec; not a second btstack.
    sh = ("#!/bin/sh\n# R94 experimental launcher wrapper; not an installer\n"
          "# Only activate when manually reviewed and deliberately placed in verified OEM supervisor config.\n"
          "umask 077\n"
          f"[ -r {shlex.quote(preload)} ] || exit 71\n"
          "AU57X_R92_PROBE=1\n"
          f"LD_PRELOAD={shlex.quote(preload)}\n"
          "export AU57X_R92_PROBE LD_PRELOAD\n"
          f"exec {shlex.quote(STOCK)} \"$@\"\n")
    if hfp_probe:
        sh=sh.replace("export AU57X_R92_PROBE LD_PRELOAD\n",
                      "AU57X_R97_GOT_PROBE=1\nexport AU57X_R92_PROBE LD_PRELOAD AU57X_R97_GOT_PROBE\n")
    report = {"version": "R97" if hfp_probe else "R94", "scope": "OFFLINE_STAGING_ONLY", "installable": False,
              "ready_for_vehicle": False, "vehicle_verified": False,
              "original_config_sha256": digest(content), "staged_config_sha256": digest(modified),
              "entry_wrapper_sha256": digest(sh.encode()), "stock_btstack_sha256": STOCK_SHA,
              "original_exec": STOCK, "staged_exec": wrapper, "preload": preload,
              "hfp_got_observer_opt_in": hfp_probe,
              "hfp_is_not_iap2": True,
              "precondition_unmodified": True, "no_second_btstack": True,
              "factory_supervisor_environment_inheritance_proven": False,
              "rfcomm_callback_bound": False,
              "warning": "Never replace car config until exact MU1005 syntax, path, permissions, launch and recovery are verified."}
    target = pathlib.Path(out_dir)
    if target.exists():
        raise ValueError("output already exists; no overwriting of an earlier rollback")
    parent = target.parent.resolve()
    parent.mkdir(parents=True, exist_ok=True)
    temp = pathlib.Path(tempfile.mkdtemp(prefix="r94_staging_", dir=parent))
    try:
        (temp / "connectivity.original.json").write_bytes(content)
        (temp / "connectivity.STAGED_REVIEW_ONLY.json").write_bytes(modified)
        (temp / "au57x_btstack_probe_entry.sh").write_text(sh)
        (temp / "au57x_btstack_probe_entry.sh").chmod(0o700)
        (temp / "R94_MANIFEST.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
        temp.rename(target)
    finally:
        if temp.exists():
            shutil.rmtree(temp)
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--config", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--expected-sha256")
    p.add_argument("--experimental-hfp-observer",action="store_true",
                   help="R97 original HFP callback observer only; not an iAP2 service")
    p.add_argument("--wrapper", default=WRAPPER)
    p.add_argument("--preload", default=PRELOAD)
    args = p.parse_args()
    try:
        report = stage(pathlib.Path(args.config).read_bytes(), args.out, args.wrapper, args.preload, args.expected_sha256, args.experimental_hfp_observer)
    except (OSError, ValueError, AssertionError, json.JSONDecodeError) as err:
        print(f"R94 DENY: {err}", file=sys.stderr)
        return 2
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
