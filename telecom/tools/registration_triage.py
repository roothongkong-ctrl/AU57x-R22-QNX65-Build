#!/usr/bin/env python3
"""Offline 3GPP registration-evidence triage for AU57x; never opens modem ports.

Inspired by public, tested registration-vs-operator-scan separation in
ModemManager and by iscle/qemu-mhi2 simulated registration traces. This is an
independent minimal implementation and is NOT an OEM modem driver.

Accepts a text transcript already copied by the user. Report contains only
high-level state, not IMSI/ICCID/IMEI/operator IDs/raw captured lines.
"""
import argparse
import json
import re
from pathlib import Path

REG_RX = re.compile(r"^\s*\+(CEREG|CGREG|CREG)\s*:\s*([0-9]+)(?:\s*,\s*([0-9]+))?", re.I)
COPS_SCAN_RX = re.compile(r"\+COPS:\s*\([^)]*\)", re.I)
COPS_READ_RX = re.compile(r"^\s*\+COPS:\s*([0-4])\s*,\s*([0-2])\s*,", re.I)
COPS_COMMAND_RX = re.compile(r"^\s*(?:AT)?\+COPS\?(\s*)$", re.I)
AT_REG_RX = re.compile(r"^(?:AT)?\+(CEREG|CGREG|CREG)\?$", re.I)
CGATT_RX = re.compile(r"^\s*\+CGATT:\s*([01])\b", re.I)
CGACT_RX = re.compile(r"^\s*\+CGACT:\s*([0-9]+)\s*,\s*([01])\b", re.I)

REG_STATES = {
    0: "NOT_REGISTERED",
    1: "REGISTERED_HOME",
    2: "SEARCHING",
    3: "REGISTRATION_DENIED",
    4: "UNKNOWN",
    5: "REGISTERED_ROAMING",
}
def status_name(n):
    return REG_STATES.get(n, "EXTENDED_OR_VENDOR_STATE")


def triage(log):
    # 'pending' is set only by a correlated query. URCs with two leading
    # numbers can be ambiguous; never guess their registration status.
    pending = None
    state = {x: None for x in ("CEREG", "CGREG", "CREG")}
    states_seen = {x: [] for x in state}
    ambiguous = 0
    scan_entries_seen = False
    cops_read_seen = False
    packet_attached_seen = None
    data_context_active_seen = False
    cme_network_service_error_seen = False
    rejinfo_seen = False
    for original in log.splitlines():
        # Handle timestamped lines from log collectors, without retaining text.
        line = re.sub(r"^.*?(?=(?:AT)?\+(?:CEREG|CGREG|CREG|COPS|CGATT|CGACT)|\+CME ERROR:|\^REJINFO)", "",
                      original.strip(), count=1, flags=re.I)
        if not line:
            continue
        q = AT_REG_RX.fullmatch(line)
        if q:
            pending = q.group(1).upper()
            continue
        if COPS_COMMAND_RX.fullmatch(line):
            pending = "COPS"
            continue
        m = REG_RX.match(line)
        if m:
            typ, first, second = m.group(1).upper(), int(m.group(2)), m.group(3)
            # If transcript includes a query, the first number is <n>.
            # If only one number is present, it is an unsolicited <stat>.
            # Without a query, two numbers are ambiguous (e.g. <n>,<stat>
            # vs <stat>,<tac> in modem-vendor variants); fail closed.
            if second is not None and pending == typ:
                st = int(second)
            elif second is None:
                st = first
            else:
                ambiguous += 1
                pending = None
                continue
            state[typ] = status_name(st)
            states_seen[typ].append(status_name(st))
            pending = None
            continue
        if line.upper().startswith("+COPS:"):
            if COPS_SCAN_RX.search(line):
                scan_entries_seen = True
            if COPS_READ_RX.search(line):
                cops_read_seen = True
            pending = None
            continue
        m = CGATT_RX.match(line)
        if m:
            packet_attached_seen = m.group(1) == "1"
        m = CGACT_RX.match(line)
        if m and m.group(2) == "1":
            data_context_active_seen = True
        if "+CME ERROR: 30" in line.upper():
            cme_network_service_error_seen = True
        if "REJINFO" in line.upper():
            rejinfo_seen = True
        if line.upper() in ("OK", "ERROR") or line.upper().startswith("+CME ERROR"):
            pending = None

    eps = state["CEREG"]
    if eps is None:
        verdict = "NO_UNAMBIGUOUS_LTE_REGISTRATION_EVIDENCE"
    elif eps in ("REGISTERED_HOME", "REGISTERED_ROAMING"):
        verdict = "LTE_REGISTERED_AT_LOG_TIME_ONLY"
    elif eps == "REGISTRATION_DENIED":
        verdict = "LTE_REGISTRATION_DENIED_AT_LOG_TIME"
    elif eps == "SEARCHING":
        verdict = "LTE_SEARCHING_AT_LOG_TIME"
    else:
        verdict = "LTE_NOT_CONFIRMED_AT_LOG_TIME"

    return {
        "scope": "OFFLINE_HISTORICAL_LOG_NOT_LIVE",
        "lte_verdict": verdict,
        "last_registration_states": state,
        "ambiguous_registration_responses": ambiguous,
        "operator_scan_rows_seen": scan_entries_seen,
        "operator_read_response_seen": cops_read_seen,
        "packet_attach_response_seen": packet_attached_seen,
        "data_context_active_response_seen": data_context_active_seen,
        "network_service_error_seen": cme_network_service_error_seen,
        "vendor_rejection_hint_seen_unparsed": rejinfo_seen,
        "runtime_network_mode_confirmed": False,
        "current_live_state_confirmed": False,
        "sim_operator_or_hardware_frequency_confirmed": False,
        "no_modem_io": True,
        "no_firmware_writes": True,
        "further_evidence": "Need correlated, fresh, read-only NAD API registration mode and rejection info, without taking over /dev/NAD/AT_port.",
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--transcript", type=Path, required=True,
                    help="local authorized diagnostic transcript (never upload raw SIM IDs)")
    ap.add_argument("--out", type=Path, required=True)
    a = ap.parse_args()
    result = triage(a.transcript.read_text(encoding="utf-8", errors="replace"))
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n",
                     encoding="utf-8")
    print(result["lte_verdict"], "(historical offline evidence only)")


if __name__ == "__main__":
    main()
