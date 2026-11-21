#!/usr/bin/env python3
"""Review-only evidence gate for AU57x China Telecom network registration.

No connection to NAD, no AT writes, no firmware modifications.
Do not commit OEM binaries or personal SIM details to a public repository.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

EXPECTED = {
    "nad": {
        "bytes": 845158,
        "sha256": "ed7202ec171deb978f07d71efcad836707be3cb9a1104ced98c51709649d3e21",
    },
    "telephone": {
        "bytes": 1458120,
        "sha256": "63a3bdcf0b681991efae19cec87583e33423d0623ae7386510f6485815c88f52",
    },
}
# Verified addresses from the *previous* OEM nad code review, not newly measured.
READ_SITE_VA = 0x18B160
COMPARE_SITES_VA = (0x18B168, 0x18B16C)
APN_RECORDS = 874
EVIDENCE_DATE = "2026-11-22"
# Not a replacement for a real NAD/network registration trace:
RUNTIME_MODE_OBSERVED = False
LIVE_CEREG_OBSERVED = False


def build_cops(mode, selection, plmn="46003"):
    """Reproduce only the four previously verified nad instruction templates.

    The two automatic templates do NOT prove the modem's eventual RAT.
    """
    if type(mode) is not int or mode < 0:
        raise ValueError("mode must be a nonnegative integer")
    if selection not in ("auto", "manual"):
        raise ValueError("selection must be auto or manual")
    if not isinstance(plmn, str) or not re.fullmatch(r"[0-9]{5,6}", plmn):
        raise ValueError("PLMN must contain 5 or 6 decimal digits")
    if selection == "manual":
        return "AT+COPS=1,2," + plmn + (",0" if mode == 1 else "")
    return "AT+COPS=0,2,,0" if mode == 1 else "AT+COPS=0,2"


def scenarios():
    # Each row is a template calculation, NOT a modem/network test.
    return [
        {"mode": mode, "selection": sel, "plmn": "46003",
         "command": build_cops(mode, sel)}
        for mode in (0, 1, 2)
        for sel in ("manual", "auto")
    ]


def _sha256(data):
    return hashlib.sha256(data).hexdigest()


def _elf_evidence(path, role):
    """Verify an authorized local OEM file, optionally produce candidate clues.

    ELF code inspection never proves caller provenance or live SIM selection.
    No OEM bytes are written into the report.
    """
    from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM
    from elftools.elf.elffile import ELFFile

    data = Path(path).read_bytes()
    expected = EXPECTED[role]
    digest = _sha256(data)
    item = {
        "role": role,
        "bytes": len(data),
        "sha256": digest,
        "expected_identity_verified":
            len(data) == expected["bytes"] and digest == expected["sha256"],
    }
    if not item["expected_identity_verified"]:
        item["inspection_skipped_reason"] = "FILE_IDENTITY_MISMATCH"
        return item

    with Path(path).open("rb") as f:
        elf = ELFFile(f)
        item["elf_class"] = elf.elfclass
        item["little_endian"] = elf.little_endian
        item["machine"] = elf.header["e_machine"]
        item["qnx_interpreter"] = None
        for seg in elf.iter_segments():
            if seg.header.p_type == "PT_INTERP":
                item["qnx_interpreter"] = seg.get_interp_name()
        item["relevant_dynamic_symbols"] = []
        dyn = elf.get_section_by_name(".dynsym")
        if dyn is not None:
            for sym in dyn.iter_symbols():
                n = sym.name
                if n and any(x in n.lower() for x in (
                        "nadmode", "nadstandardnetwork", "networkselection",
                        "requestnetworkregistration", "selectnetwork")):
                    item["relevant_dynamic_symbols"].append(n[:160])
            item["relevant_dynamic_symbols"] = sorted(
                set(item["relevant_dynamic_symbols"]))[:60]

        if role == "nad":
            # Check exact byte strings separately: the shorter template is a
            # substring of the longer one, hence one hit is not independent proof.
            for key, marker in {
                "cops_manual_gsm": b"AT+COPS=1,2,%s,0",
                "cops_manual_unrestricted": b"AT+COPS=1,2,%s\x00",
                "cops_auto_gsm": b"AT+COPS=0,2,,0",
                "cops_auto_unrestricted": b"AT+COPS=0,2\x00",
            }.items():
                item[key] = marker in data

            # These VAs were documented in earlier analysis. Disassemble around
            # read/compare sites and collect *candidate* offset-0x10 writers.
            text = elf.get_section_by_name(".text")
            item["nad_mode_read_site"] = "NOT_MAPPED"
            item["mode_write_candidates_heuristic"] = []
            if text is not None:
                base = int(text["sh_addr"])
                raw = text.data()
                end = base + len(raw)
                cs = Cs(CS_ARCH_ARM, CS_MODE_ARM)
                if base <= READ_SITE_VA and READ_SITE_VA + 4 <= end:
                    lo = max(0, READ_SITE_VA - 48 - base)
                    hi = min(len(raw), READ_SITE_VA + 84 - base)
                    ins = list(cs.disasm(raw[lo:hi], base + lo))
                    item["nad_mode_read_site"] = [
                        {"va": hex(i.address), "mnemonic": i.mnemonic,
                         "operands": i.op_str}
                        for i in ins
                        if READ_SITE_VA - 12 <= i.address <= READ_SITE_VA + 28
                    ]
                    item["read_site_observed"] = any(
                        i.address == READ_SITE_VA and
                        i.mnemonic.startswith("ldr") and
                        "r5" in i.op_str and "#0x10" in i.op_str
                        for i in ins
                    )
                    item["compare_sites_mapped"] = [
                        hex(i.address) for i in ins
                        if i.address in COMPARE_SITES_VA
                    ]
                else:
                    item["read_site_observed"] = False
                    item["compare_sites_mapped"] = []
                # This can include unrelated functions, so NEVER label as
                # the actual mode setter without control/data-flow proof.
                candidates = []
                for i in cs.disasm(raw, base):
                    if i.mnemonic in ("str", "strb", "strh") and re.search(
                        r"\[[^\]]+,\s*#0x10\]", i.op_str
                    ):
                        candidates.append(hex(i.address))
                item["mode_write_candidate_count_unverified"] = len(candidates)
                item["mode_write_candidates_heuristic"] = candidates[:40]
            item["mode_source_confirmed"] = False

    return item


def audit(nad=None, telephone=None):
    result = {
        "project": "AU57x_CHINA_TELECOM_R01",
        "scope": "OFFLINE_REVIEW_ONLY",
        "evidence_date": EVIDENCE_DATE,
        "report_status": "EVIDENCE_ONLY",
        "firmware": "MHI2_CN_AU57x_K1006_2/MU1005",
        "apn_original_records": APN_RECORDS,
        "apn_telecom_entries_missing_in_original": ["46003", "46005", "46011", "46012"],
        "baseline_observations": {
            "chn_ct_seen_in_scan": True,
            "lte_registration_proven": False,
            "runtime_mode_proven": RUNTIME_MODE_OBSERVED,
            "live_cereg_proven": LIVE_CEREG_OBSERVED,
            "adding_ctnet_alone_resolved_problem": False,
        },
        "mode_templates": scenarios(),
        "files": [],
        "mode_source_confirmed": False,
        "live_modem_test": False,
        "installer_produced": False,
        "mobile_sim_compatibility_protected_by_no_firmware_changes": True,
        "next_blocker": "Trace actual mode assignment across telephone/NADServicesProxy/NadStandardNetworkManager; requires OEM binary or runtime evidence.",
    }
    for role, path in (("nad", nad), ("telephone", telephone)):
        if path is not None:
            result["files"].append(_elf_evidence(path, role))
    return result


def report_md(result):
    files = result["files"]
    lines = [
        "# 中国电信兼容 R01 — GitHub 云端离线审查",
        "",
        "**状态：EVIDENCE_ONLY（不是车机联网成功）**",
        "",
        "- 车辆：2017 奥迪 A6L，MHI2_CN_AU57x_K1006_2/MU1005。",
        "- 已有实车证据：搜到 CHN-CT，手动连接后“无应答”；未证明 LTE 注册。",
        "- 证据边界：当前未取得实时 mode、CEREG/REJINFO、模块射频能力。",
        "- APN：原厂 874 条，缺电信条目；单补 ctnet 不足以解决。",
        "- 本轮不修改原厂二进制、不写 AT、不更改中国移动配置。",
        "",
        "## 原厂分支模板（模拟，不是实车执行）",
        "",
        "| mode | 选网 | 指令 |",
        "|---|---|---|",
    ]
    for row in result["mode_templates"]:
        lines.append("| %d | %s | \`%s\` |" % (
            row["mode"], row["selection"], row["command"]))
    lines += ["", "## 原厂 ELF 样本校验", ""]
    if not files:
        lines += [
            "本次 GitHub Actions 未提供专有原厂 ELF；仅运行源码和测试。",
            "无法从这个绿勾推出 mode 的来源或电信卡在真实车上的值。",
        ]
    else:
        for item in files:
            lines.append("- %s：SHA256/长度匹配=%s；动态符号线索=%d。" % (
                item["role"], item["expected_identity_verified"],
                len(item.get("relevant_dynamic_symbols", []))))
            if item["role"] == "nad":
                lines.append(
                    "  - mode 读点是否命中：%s；写点候选数（未证明）：%s。" %
                    (item.get("read_site_observed", False),
                     item.get("mode_write_candidate_count_unverified", 0))
                )
    lines += [
        "",
        "## 当前阻塞",
        "",
        "**mode 参数传递与赋值来源仍未得到证实。** 不得推断电信卡必然走 GSM 分支。",
        "真实 LTE 注册必须最后在车辆/基带及运营商网络上验证。",
        "",
    ]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nad", type=Path, default=None)
    parser.add_argument("--telephone", type=Path, default=None)
    parser.add_argument("--out", type=Path, default=Path("telecom_result"))
    args = parser.parse_args()
    result = audit(nad=args.nad, telephone=args.telephone)
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "status.json").write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8")
    (args.out / "RESULTS_CN.md").write_text(
        report_md(result), encoding="utf-8")
    print("R01 %s; %d template cases; %d OEM files inspected" %
          (result["report_status"], len(result["mode_templates"]),
           len(result["files"])))


if __name__ == "__main__":
    main()
