# X4.3 R108 — X4.2 Owner Gate Port + R113 SCN Upgrade
Date: 2026-11-26
Status: SOURCE_ONLY / HOST_FIXTURE_PASS / QNX_COMBINED_BUILD_PENDING / NO_INSTALL

## What was actually integrated into x4.3-integration-plan-20261126
1. Upgraded project/source/legacy_mu1005/bluetooth/inbound.c and inbound.h to the exact R113/R116 blobs. This adds the original MU1005 RFCOMM SCN range 1..30, validates the returned SCN, and requires the SDP protocol SCN to match before SDP publication.
2. Added tests/test_mu1005_rf_scn_extended.c and ci/host_mu1005_rf_scn_extended.sh, plus a branch-scoped host workflow.
3. Ported the X4.2 X3 owner evidence gate into x3/src/mu1005_owner_gate.c/h.
4. Ported X4.2 rf_arg_experiment.c/h, src/bluetooth/native_binding.c/h, and updated bt_iap2_owner_handoff.c/h to require X3 owner evidence, server-role proof, an independently attested iAP2 arg3/SCN and a frozen snapshot BEFORE opening the local TCP bridge. A failed TCP connect clears the snapshot. The native QNX branch rechecks the same owner generation and SCN before callbacks/ticks.
5. Updated tests/test_r98_iap2_owner_handoff.c and added ci/host_x4_3_native_regression.sh / .github/workflows/x4.3-host-regression.yml.

## Source provenance
- R113 dependency blobs already present in the X4.3 branch were compared and match the R113 branch: inbound_r5.c/h, native_event.c/h, native_binding_words.h, mu1005_rf_ingress.c/h, mu1005_ipc_stream.c/h and mu1005_socket_pump.c/h.
- The R113 inbound.c/h blobs are now copied into X4.3 exactly: inbound.c 81bececdbe51fefc0a033c9ca74dd97aabf64a8d; inbound.h 4c5fe1225543d135ed7a24c56a8b02fbf2c49252.
- X4.2 owner-gate source is now in the X4.3 branch; it is no longer only an archive reference.

## Tests and evidence
- Local run of the archived X4.2 host fixture suite `ci/host_ae_gate_snapshot.sh`: PASS. Output: `R98 REAL LOOPBACK OWNER GATE PASS: authority, live fd, rollback, poison`; `X3 owner gate: 17/17 assertions passed`. The script also compiled the native_binding non-QNX host stub. This is host-only synthetic evidence, not OEM runtime.
- The R113/R116 GitHub Actions run 45208696553 separately passed the R113 native owner publication host integration, QNX 6.5 ARMv7 link of the R113 owner layer, ELF/dependency checks and QEMU smoke. It does NOT compile the newly combined X4.3 X4.2 native_binding QNX branch.
- The new X4.3 branch workflow has been added, but no completed run/status is available yet. Do not report X4.3 GitHub CI PASS until a run ID and successful job logs are observed.
- The focused SCN 1..30 test is already part of the R113/R116 workflow that passed. A local equivalent focused compile/run also passed, but the newly added X4.3 workflow remains pending.

## Remaining critical blockers
1. The X4.3 combined `native_binding.c` QNX ARM32 branch has not yet been compiled and linked with all production headers/sources. Host stub compile is not QNX compile.
2. The external `MuBtOwner`/`Au57xX3Evidence` production provider is absent. No real btstack iAP2 owner thread, RFCOMM arg3 ABI, SCN mapping, callback binding or teardown barrier has been captured. Therefore the gate correctly denies native attach.
3. The native binding uses pinned absolute addresses guarded by instruction words and owner evidence; no live mapped-image/runtime proof authorizes execution. Do not run these function pointers in a vehicle.
4. SI/DIO production callback source and same-phone Bluetooth MAC/Wi-Fi station/AirPlay session binding remain unproven.
5. Real iPhone MFi, Wi-Fi handoff, AirPlay media/control, USB takeover/resume and vehicle wireless session remain untested.

## Next exact step
A dedicated QNX 6.5 ARMv7 build script and GitHub Actions workflow have now been added: `ci/compile_x43_native_qnx.sh` and `.github/workflows/x4.3-qnx-native-build.yml`. It compiles/links the complete native path (`native_binding.c`, `native_event.c`, `inbound_r5.c`, `inbound.c`, RF/IPC/socket pumps, owner handoff, route gate and X3 gate), then runs ELF/NEEDED checks. The workflow run is not yet visible, so this remains PENDING. QEMU is intentionally not asked to execute the fixed-address native attach path; this target validates ABI/compilation only, not original owner/MFi.

NO_INSTALL / safe_to_flash=false / vehicle_verified=false.

## R109 update — target build gate created
The full X4.3 QNX ARM32 build script and workflow are now in the branch. Pending evidence: a successful workflow run ID plus the uploaded ELF gate/readelf artifact. Until then, do not claim combined QNX compile PASS.

## R110 — QNX/host build gate static audit corrections
Before relying on the new workflow, a source-path audit found that both build scripts referenced `src/bluetooth/bt_route_gate.c`, but the real source lives at `project/source/legacy_mu1005/bluetooth/bt_route_gate.c`. Both `ci/compile_x43_native_qnx.sh` and `ci/host_x4_3_native_regression.sh` now reference the real source.
A second integration defect was found in `mu38_dispatch`: it compared the `Mu38PeerOps *` structure address with `b->vendor.opaque`, although the intended relation is `o->opaque == b->vendor.opaque` (Resident embeds Mu38PeerOps as its first member). The check is corrected, and `tests/test_native_event_context.c` was added to guard the owner-context contract.
The isolated native-event context regression compiled and passed with a local host fixture. This is a focused test only; it is not the complete branch host workflow and does not imply QNX compilation.

## Current gate status after R110
- Source-path corrections: DONE.
- Native-event resolver context correction: DONE.
- Focused isolated host regression: PASS.
- Full X4.3 host workflow: PENDING / no verified run result in this review.
- Full X4.3 QNX 6.5 ARM32 compile/link + ELF gate: PENDING / no verified run result in this review.
- Production OEM btstack owner provider, MFi, AirPlay and vehicle validation: NOT PROVEN.
NO_INSTALL / safe_to_flash=false.