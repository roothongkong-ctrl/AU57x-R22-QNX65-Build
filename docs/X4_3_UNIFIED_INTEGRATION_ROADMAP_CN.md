# X4.3 AU57x Wireless CarPlay — Unified Integration Roadmap
Date: 2026-11-26
Target: 2017 Audi A6L C7 PA / MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARMv7
Branch: x4.3-integration-plan-20261126
Base: mu1005-wireless-implementation-r103-20261125

## Mission
Deliver native, stable wireless CarPlay on AU57x/MU1005 while preserving the existing wired CarPlay path and the user's already-working main-screen + instrument-cluster map display. Use Bluetooth for bootstrap/iAP2 as required by the proven OEM flow, Wi-Fi for the high-bandwidth session, the stock MFi/AirPlay/DIO/SI stack wherever the MU1005 evidence supports it, and preserve OEM authentication. No MFi bypass, donor firmware transplant, guessed OEM offsets, or unsafe replacement of the stock USB transport.

## User-approved product requirements
1. Wireless CarPlay uses Bluetooth + Wi-Fi; no internet connection is required for the local CarPlay link.
2. First pairing may require user authorization; subsequent vehicle starts should connect automatically.
3. Prefer the last successfully used iPhone. If unavailable, try another already-paired/authorized phone. Never erase unrelated pairing records.
4. On leaving range, end the session cleanly; on return, reconnect without pairing again.
5. Retry a bounded number of times; only after retries fail show a manual-connection prompt.
6. Preserve stock wired CarPlay. Wired USB has priority; after USB removal, wireless may resume automatically once the stock wired teardown/restore barrier is confirmed.
7. Preserve the current main MMI CarPlay display and the existing instrument-cluster navigation/map display, plus touch, rotary, steering-wheel, audio and call behavior. Do not redesign or replace the user's existing display configuration.
8. Final install, backup, validation, rollback and Chinese instructions are required, but only after all technical and safety gates pass.

## Evidence-based baseline (do not overstate)
- R103: upstream LuKa iAP2 LinkEngine source was pinned by Git blob hashes; host upstream 7/7, bridge 2/2, TCP lab 13/13 passed. This is host evidence, not QNX Rust or OEM Bluetooth evidence.
- QNX C research build: GitHub Actions run 45634440000 passed GCC 8.5 ARM EABI5 compile/link, strict ELF gate and QEMU dlopen smoke. It did not call OEM APIs and does not establish a complete wireless runtime.
- R104: original MU1005 ipod-drvr-iap2.so static analysis located iap2_init, init_features, bt_start_updates, bt_update_recv, bt_send_info and transport_send/recv abstractions. bt_update_recv is a structured Bluetooth state/PPS update parser, not proven raw RFCOMM ingress. The raw RFCOMM producer and Transport object source remain unresolved.
- R105/R106: real vehicle snapshot proves uap0 2.4 GHz AP operation and a station association; wired USB CarPlay path reaches DIO/AirPlay receiver readiness. This does not prove the station is the target iPhone, nor wireless iAP2/MFi/AirPlay session.
- R104 USB Transport ABI work: static analysis of the original usbdevice plugin recovered the module/transport object layout and send/receive function-table offsets. This describes the mm-ipod physical Transport plugin interface; it must not be conflated with iAP2's internal BluetoothTransportComponent. The original startup still selects -t usbdevice. Do not replace it.
- R143 observer: separate exploratory branch. Its checked status file still reports pending CI and no verified OEM Bluetooth owner/iAP2/MFi/AirPlay. It is optional evidence tooling and must not block the main tracks.
- X4.2: a user-library snapshot reports owner-evidence gate and pre-TCP owner snapshot wiring with synthetic tests. The production OEM evidence provider remains absent. Reconcile the exact X4.2 source patch against this branch before cherry-picking; do not infer it is already in this branch.
- Earlier R41/R44/R71-R86/R95-R103 artifacts provide reusable MFi boundary research, SI/DIO session model, delegate ABI analysis, wired recovery model, transaction/rollback tests and source provenance. Host fixtures remain fixtures.

## External research watchlist (architecture/source reuse only)
1. congobg/MHI2Q_WiFi_CarPlay — project status dated 2026-10-27 reports wireless operation on its MHI2Q QC firmware 5092/3638, stock receiver/iAP2/HMI reuse, Wi-Fi channel handoff, Bonjour, session iAP2 and wired/wireless behavior. Strong end-to-end workflow reference, but different MHI2Q QC firmware; no binary/script/offset transplant to AU57x.
2. joeyQuery/MHI2-wirelessCarPlay — current investigation shows a useful Bluetooth bootstrap → IapDeviceServices → CIapBTChannel → runtime endpoint → Wi-Fi AP → AirPlay/Bonjour → DIO chain. Its current unresolved Bluetooth endpoint owner and AirPlay interface selection are directly relevant research questions, not proof for MU1005. Compare each edge with MU1005 ELF/log evidence.
3. luka-dev/mib2q-carplay-rgi — continue reusing only the pinned protocol-level iAP2 source under its license; never transplant fixed addresses or MHI2Q-specific OEM bindings. Latest public release tag observed: MHI2Q-2026-11-04; useful for cluster-map reconnect/display behavior, not a MU1005 wireless solution.
4. maxim-smirnov/mhi2-carplay-cluster and related MHI2Q cluster projects — useful for map-to-cluster display, controls and test-lab patterns. Different MHI2Q target and may be adapter-dependent; reuse concepts/source only after license and dependency audit.
5. Continue broad GitHub/community tracking for newly published source, patches, release notes, test logs and reports. Record repository, commit/tag/date, license, target firmware, exact reusable files, non-reusable OEM binding, and evidence class.

## Unified architecture to validate (not assume)
Phone discovery/pairing
  -> OEM Bluetooth bootstrap / iAP2 service registration
  -> real RFCOMM endpoint and phone identity
  -> iAP2 LinkEngine / stock iAP2 component (choose based on OEM evidence)
  -> OEM-approved MFi authentication/session
  -> Wi-Fi credentials/channel handoff and association
  -> Bonjour/AirPlay discovery and session negotiation
  -> stock MU1005 AirPlay/DIO receiver and SI owner
  -> existing MMI display/audio/control + existing cluster map route
  -> session teardown / USB priority / OEM restore barrier

Do not assume the physical mm-ipod Transport plugin is necessarily the Bluetooth path. Determine whether MU1005's BluetoothTransportComponent already owns a working iAP2 channel, whether it exposes a runtime endpoint to another process, or whether a separate physical Transport plugin is required. Only one conclusion may be selected after static and runtime evidence.

## Workstreams (parallel, merge only through reviewed interfaces)
### A. Source and provenance consolidation
- Inventory every branch from R86-R143, R103, R104/R105 and X4.2 snapshot.
- Compare files by path + blob SHA; classify as reusable, superseded, conflicting, fixture-only, or OEM-private.
- Preserve source/license notices. Do not copy PolyForm Noncommercial donor code into distributable source without legal review. Do not publish OEM binaries, private firmware, keys, pairing secrets or QNX SDK.
- Create one integration branch with manifest, evidence ledger, reproducible build commands and no binary payloads.
### B. Bluetooth / RFCOMM / OEM owner (critical path)
- Continue R104 caller/callee and PLT/GOT cross-reference work for iap2_init, transport_recv_pkt, transport_send_pkt, link_get_transport_data, bt_start_updates and bt_send_info.
- Decode iap2.cfg component-selection keys and object construction, then map them to original Bluetooth callback/owner.
- Establish exact service/profile, SDP SCN, registration arguments, callback ABI, thread/owner, lifetime, unregister and error behavior from pinned MU1005 samples or read-only vehicle evidence.
- Use R143 observer only if its source and CI are reconciled; no guessed /proc paths or OEM calls.
- Do not bind R103 LinkEngine to bt_update_recv.
### C. iAP2 and MFi
- Keep the pinned upstream LinkEngine and existing C bridge behind a stable transport interface.
- Verify QNX ARM feasibility separately (QNX Rust target/toolchain or a bounded C ABI alternative); do not call host TCP lab a QNX runtime.
- Map stock MFi API ownership, handles, memory release, thread affinity and peer-auth-success callback.
- Require real same-phone/same-session/same-generation evidence before auth/session readiness.
- No certificate/key bypass or spoofed success.
### D. Wi-Fi / radio
- Start from the proven 2.4 GHz uap0 path and read live AP state/channel/address at runtime.
- Confirm station association, target phone identity, IP assignment, routes and firewall reachability.
- Investigate 5 GHz only after loaded firmware, driver capabilities, regulatory domain and RF front-end are all supported by evidence; automatic fallback must be capability-driven.
- No external internet required.
### E. Bonjour / AirPlay
- Determine existing mDNS daemon/service owner and exact AirPlay service TXT/identity from MU1005.
- Reuse stock receiver; prove session negotiation, authentication, audio/video/control channels and teardown.
- Listener bind or Bonjour advertisement alone is not session success.
### F. SI / DIO / display / cluster / controls
- Reconcile R71-R86 delegate ABI and SI/DIO owner lifecycle with actual MU1005 callbacks and peer identity.
- Preserve user's current MMI and cluster map configuration. Audit existing cluster display path before any wireless change.
- Require same-session video, audio, MMI, steering/rotary and cluster-map evidence.
- Wired USB takeover wins; restore is reported only after stock SI teardown and DIO restore are both confirmed.
### G. QNX build / test / integration
- Reuse the existing QNX 6.5 ARMv7 build container and strict ELF gate.
- Build each production-target module with actual QNX sysroot; record NEEDED, SONAME, symbols, relocations and ABI.
- Keep host fixtures, QEMU synthetic smoke, OEM runtime and vehicle tests in separate evidence classes.
- Merge only after interface contracts and regression tests pass.

## Ordered execution plan and gates
P0 — Baseline freeze and branch reconciliation:
- Diff all relevant branches and X4.2 snapshot against R103 baseline.
- Import only exact passing patches; document excluded conflicts.
- Output a unified file/commit manifest and evidence ledger.

P1 — OEM Bluetooth/iAP2 ingress proof (highest priority):
- Complete R104 static call graph and config mapping.
- Establish whether stock BluetoothTransportComponent already handles real RFCOMM and how its endpoint reaches iAP2.
- Deliver a function/process/thread/IPC/endpoint chain with pinned binary hashes and caller/callee evidence.
- If no production evidence exists, keep the adapter fail-closed; no plugin implementation based on guesswork.

P2 — Build the smallest reversible transport adapter:
- Select stock internal Bluetooth path or physical plugin only after P1.
- Preserve stock -t usbdevice and wired startup.
- Implement only a separately loadable, disabled-by-default experiment with explicit stop/unload/rollback; no production enablement without real producer evidence.
- Validate host, QNX ARM and QEMU separately.

P3 — iAP2/MFi session:
- Connect real OEM producer to LinkEngine or stock engine according to P1.
- Verify phone identity, auth, retransmit, disconnect, retry and generation.
- MFi readiness only from stock OEM authentication result.

P4 — Wi-Fi + Bonjour + AirPlay:
- Establish phone handoff, AP channel correctness, network reachability and AirPlay session.
- Verify media/control end-to-end on same session.
- 2.4 GHz first; 5 GHz optional and capability-gated.

P5 — SI/DIO + existing cluster behavior:
- Reuse the stock receiver and existing user's display setup.
- Verify MMI and instrument-cluster map simultaneously, audio, touch, knob and steering controls.
- Verify wired takeover, USB removal and wireless resume with full OEM restore barrier.

P6 — End-to-end reliability:
- Reconnect, out-of-range, multiple paired phones (last successful phone first), bounded retry and manual prompt, cold boot, sleep/wake, long-run and error recovery.
- Tune retry count/timing only from measured behavior; keep safe defaults.

P7 — Release and installation:
- Source audit, license audit, complete QNX ELF/ABI checks, host/QEMU regression, OEM runtime proof, real iPhone/MFi/AirPlay vehicle test, wired regression and rollback drill.
- Only then build SD installer with automatic full backup, manifest/hash verification, install, reboot check, rollback and Chinese/pinyin prompts.
- Until every required gate passes: RESEARCH_ONLY / NO_INSTALL / safe_to_flash=false.

## Acceptance evidence classes
H0 provenance/license/source hash
H1 host tests/sanitizers/fuzz
H2 QNX ARM target ELF/ABI build
H3 QEMU load/run/exit/log
H4 MU1005 OEM runtime process/thread/callback/session evidence
H5 real iPhone Bluetooth/iAP2/MFi/Wi-Fi/AirPlay session
H6 MMI + cluster map + controls + wired takeover/restore + rollback

H0-H3 are development evidence only. Wireless CarPlay is not end-to-end complete until H4-H6 pass.

## Immediate next batch
1. Produce branch-by-branch diff/manifest for R103, R104/R105, R143 and X4.2; no blind merge.
2. Finish R104's static cross-reference/config decode and generate a machine-readable OEM iAP2 transport call graph.
3. Independently compare joeyQuery's latest endpoint trace against MU1005, marking each edge SAME / DIFFERENT / UNKNOWN.
4. Audit congobg's 2026-10-27 complete workflow and issue fixes as architecture reference; extract only generic, license-compatible ideas.
5. Add CI gates for the evidence ledger and prohibit mock/host results from being promoted to OEM/vehicle PASS.
6. R106 correction: R99's two config inputs were explicitly NOT_OEM synthetic fixtures. Do not count R99 as real OEM config validation; obtain the genuine K1006_2/MU1005 Bluetooth JSON and iap2.cfg before claiming config-parser progress.
6. Continue Wi-Fi, AirPlay, SI/DIO and cluster-display evidence work in parallel; do not let the Bluetooth critical path prevent independent static work.

## Current status
Unified plan and branch created; source code has NOT yet been merged into one production-ready implementation. R103 remains the pinned protocol-link core, but is no longer the only source baseline. R113/R116 is the newest QNX/QEMU-tested native inbound/publication layer; X4.2 contributes the X3 owner gate and pre-TCP arg3/SCN snapshot contract. Reconcile these selectively: the R113 branch is diverged from R103 (37 ahead / 68 behind), so do not merge wholesale. R99 config staging was synthetic-only and does not validate genuine OEM config. No real btstack owner, MFi success, wireless CarPlay session, or vehicle validation is claimed. NO_INSTALL / safe_to_flash=false.

## R107 — current source integration gate
1. R113 native_event / InboundR5 / socket-pump dependencies were already present in the X4.3 branch and their blobs match the R113 branch. X4.3 inbound.c/h have now been upgraded to R113's SCN 1..30 implementation; a focused regression and host workflow were added (new branch workflow run still pending).
2. X4.2 native_binding.c/h, X3 owner gate, rf_arg_experiment and pre-TCP R98 snapshot handoff are now ported into X4.3. Host fixture sources and a new branch workflow are added.
3. Compile the complete X4.3 native_binding QNX ARM32 branch using production headers/sources. The existing R113 QNX pass only covers its owner publication layer; the X4.2 host fixture/stub compile is not a substitute. The new X4.3 workflow has no completed run status yet.
4. Reconcile R116 SUSPENDED/withdrawal lifecycle with X4.2 owner gate and generation/SCN freeze.
5. Trace the real MU1005 btstack iAP2 owner and registration callback. No production provider means fail-closed; no fixed-address attach is permitted.
6. Map genuine SI/DIO callbacks and same-iPhone session identity separately from the Bluetooth transport work.
7. Keep native Wi-Fi 0x5703 as an OEM candidate path; do not introduce a second 0x5703 sender until the single iAP2 session owner is proven.

## R108 — latest integration checkpoint
The X4.3 branch now contains the X4.2 owner gate/pre-TCP snapshot contract and the R113 inbound SCN 1..30 source update. The archived X4.2 host fixture suite passed locally (R98 loopback, X3 17/17, host stub compile). The R113 QNX/QEMU run remains separate; the combined X4.3 native_binding QNX build and production OEM owner provider are still blocked. See `docs/X4_3_R108_X42_OWNER_GATE_PORT_AND_R113_SCN_UPGRADE_CN.md`.


## R109 — QNX target build gate
A full X4.3 native-binding QNX 6.5 ARM32 compile/link script and workflow have been added (`ci/compile_x43_native_qnx.sh`, `.github/workflows/x4.3-qnx-native-build.yml`). It intentionally stops at ELF/NEEDED validation and does not execute fixed-address OEM callbacks under QEMU. The workflow result is pending; no combined QNX PASS is claimed yet.


## R110 — build-gate corrections
Static audit corrected both host and QNX build scripts to compile the actual `project/source/legacy_mu1005/bluetooth/bt_route_gate.c` path. It also fixed the native event resolver owner-context comparison (`o->opaque` vs the owner context) and added a focused regression test. The isolated host fixture passed; full branch host CI and combined QNX ARM32 workflow results are still pending. No runtime or vehicle claim is made.
