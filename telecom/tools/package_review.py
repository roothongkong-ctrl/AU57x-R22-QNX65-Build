#!/usr/bin/env python3
"""Build a source/review/SD-probe ZIP. NEVER produces a vehicle installer."""
import argparse
import hashlib
import json
import zipfile
from pathlib import Path

ARCHIVE_ROOT = "AU57x_CT_R03_REVIEW_ONLY"
STAMP = (2026, 11, 22, 0, 0, 0)
README = """AU57x 中国电信联网 R03 云端研究包（绝非安装包）

车型：2017 奥迪 A6L C7，MHI2_CN_AU57x_K1006_2 / MU1005。

1. 这是 GitHub Actions 离线研究成果，不代表车机 LTE 电信联网成功。
2. SD_PROBE/READ_ONLY_PROBE.sh 仅为将来人工审核时使用的 SD 只读报告脚本；
   目前无需上车，也不要直接当成 M.I.B 安装脚本运行。
3. DO_NOT_INSTALL/INSTALL_DISABLED.sh 明确拒绝安装，退出码为 78。
4. RELEASE_GATE.json 标记了仍未满足的修复发布要求。
5. 不包含任何原厂 nad/telephone 程序、不包含固件、不含真实 SIM 标识、
   不会改 APN、不会发出 AT 指令、不会更改基带。
6. 原始文件分析独立完成；本 ZIP 只含可公开的分析记录、脚本及检验结果。
7. 中国移动仍须在任何未来补丁的实车回归中验证。

如未找到真实故障根因和专门修复，禁止把本包命名为上车安装包。
"""


def sha(buf):
    return hashlib.sha256(buf).hexdigest()


def add_bytes(zipout, name, data):
    if name.startswith("/") or ".." in name.split("/"):
        raise ValueError("UNSAFE_ARCHIVE_PATH")
    info = zipfile.ZipInfo(ARCHIVE_ROOT + "/" + name, STAMP)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o100644 << 16
    zipout.writestr(info, data)


def build(base, result_dir, output):
    base, result_dir, output = map(Path, (base, result_dir, output))
    spec = {
        "SD_PROBE/READ_ONLY_PROBE.sh": base / "sd/READ_ONLY_PROBE.sh",
        "DO_NOT_INSTALL/INSTALL_DISABLED.sh": base / "sd/INSTALL_DISABLED.sh",
        "REPORTS/R01_RESULTS_CN.md": result_dir / "RESULTS_CN.md",
        "REPORTS/RELEASE_GATE.json": result_dir / "release_gate.json",
        "GITHUB_R02_OPEN_SOURCE_CN.md": base / "docs/R02_GITHUB_REFERENCES_CN.md",
        "R03_ORIGINAL_ARM_EVIDENCE_CN.md": base / "docs/R03_OEM_MODE_PATH_CN.md",
    }
    result = json.loads(spec["REPORTS/RELEASE_GATE.json"].read_text(encoding="utf-8"))
    if result.get("safe_to_build_vehicle_installer") is not False:
        raise ValueError("CANNOT_PACKAGE_UNBLOCKED_INSTALL_GATE_AS_REVIEW_ONLY")
    script = spec["DO_NOT_INSTALL/INSTALL_DISABLED.sh"].read_text()
    if "exit 78" not in script or "AU57X_CT_INSTALL_BLOCKED" not in script:
        raise ValueError("DISABLED_INSTALLER_NOT_PROVABLY_DISABLED")
    probe = spec["SD_PROBE/READ_ONLY_PROBE.sh"].read_text()
    if "TARGET_MUTATIONS=NONE" not in probe or "AT+COPS=" in probe:
        raise ValueError("PROBE_NOT_READ_ONLY")
    payload = {"README_CN.txt": README.encode("utf-8")}
    for name, path in spec.items():
        if not path.is_file() or path.is_symlink():
            raise ValueError("INPUT_NOT_REGULAR:" + name)
        payload[name] = path.read_bytes()
    for name in payload:
        if Path(name).suffix.lower() in (".bin", ".so", ".img", ".elf", ".7z", ".pfx", ".key"):
            raise ValueError("PRIVATE_BINARY_OR_KEY_REFUSED")
    manifest = "".join(sha(data) + "  " + name + "\n"
                       for name, data in sorted(payload.items())).encode()
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, mode="w") as z:
        for name, data in sorted(payload.items()):
            add_bytes(z, name, data)
        add_bytes(z, "SHA256SUMS.txt", manifest)
    with zipfile.ZipFile(output) as z:
        for line in manifest.decode().splitlines():
            digest, name = line.split("  ", 1)
            if sha(z.read(ARCHIVE_ROOT + "/" + name)) != digest:
                raise ValueError("PACKAGE_HASH_VERIFICATION_FAILED")
    return {"archive": str(output), "entries": len(payload) + 1,
            "bytes": output.stat().st_size,
            "installer_enabled": False, "oem_binaries_included": False}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--base", type=Path, default=Path("telecom"))
    p.add_argument("--result-dir", type=Path, default=Path("telecom_result"))
    p.add_argument("--out", type=Path, default=Path("telecom_result/AU57x_CT_R03_REVIEW_ONLY.zip"))
    a = p.parse_args()
    print(json.dumps(build(a.base, a.result_dir, a.out), ensure_ascii=False))


if __name__ == "__main__":
    main()
