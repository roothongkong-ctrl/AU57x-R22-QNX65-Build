# X4.3 R107 — R113/R116 + X4.2 + X3 Source Reconciliation
Date: 2026-11-26
Status: RESEARCH_ONLY / NO_INSTALL / safe_to_flash=false

## Executive result
X4.3 must not continue from R103 alone. There are three useful but different prior work lines:
- R103: pinned upstream iAP2 LinkEngine/wire framing reuse; protocol-neutral link core.
- X4.2: source-level X3 owner gate and R98 pre-TCP arg3/SCN snapshot ordering; host fixture only, no full QNX ARM32 build of its native_binding path.
- R113/R116: newer native inbound + iAP2 stream join, safe withdrawal and QNX/QEMU validation; actual original btstack owner and MFi still unknown.
These should be selectively reconciled. Do NOT merge the R113 branch wholesale: GitHub compare from R103 baseline to r113-r116-cloud-review is diverged, 37 commits ahead and 68 behind.

## 1. R113/R116 cloud evidence — what actually passed
Latest reviewed GitHub workflow run 45208696553 completed successfully. Steps included: source provenance guard; host core regression + sanitizers; MU1005 inbound SCN 1..30 regression; R113 publication guard; native inbound -> R113 owner integration; pinned QNX 6.5 ARMv7 compiler image; QNX cross-link of the original public bridge; QNX target link of the R113 owner publication layer; ELF/dependency checks and QEMU smoke.
R116 archive status records 83/83 host assertions, 13 Python tests, GCC/Clang UBSAN PASS, and QNX baseline bridge + R113 cross-compile/QEMU smoke PASS. This is meaningful target ABI/build progress.
Scope limit: the workflow's QNX owner step links r113/src/au108_oem_port.c, r113/src/mu1005_native_gate_adapter.c and r105_owner_gate.c with inbound.o. It does NOT compile the X4.2 src/bluetooth/native_binding.c QNX branch, does NOT provide a genuine original btstack owner callback, and does NOT perform iPhone/MFi/AirPlay or vehicle wireless validation. R116_STATUS explicitly keeps actual OEM Bluetooth iAP2 owner, MFi authentication and wireless CarPlay false.

## 2. X3 audit reconciliation against X4.2
X3_AE_MU1005_OWNER_DIO_AUDIT_CN.md reviewed X4.1 and found: (a) X3 gate not called from native attach, (b) missing production dependencies in X4.1 ZIP, and (c) snapshot ordering mismatch.
X4.2 archive fixes (a) at source-contract level: au98_publish calls au57x_x3_owner_gate before opening TCP, and native_binding.c's QNX branch rechecks the evidence gate in attach/cert. This remains a synthetic evidence provider contract; no production provider exists.
X4.2 fixes (c) at source-contract level: it obtains the attested candidate arg3/SCN and X3 evidence, freezes rf_arg3_snapshot before creating the local TCP socket, and clears the snapshot if connect fails. This has host fixture coverage only.
Finding (b) remains open. The X4.2 ZIP has only tests/stubs/native_event.h; production native_event.h/c and native_binding_words.h are absent. It also lacks the full InboundR5 dependency chain required by native_binding.c. Therefore its native_binding.c compile is only a non-QNX host stub compile, not a full QNX ARM32 native attach build.

## 3. Recoverable source set found in R113 branch
The r113-r116-cloud-review branch contains the following production-source dependencies that can be selectively restored into the X4.3 working tree:
- project/source/legacy_mu1005/bluetooth/inbound.c/h
- inbound_r5.c/h
- native_event.c/h
- native_binding_words.h
- mu1005_rf_ingress.c/h
- mu1005_ipc_stream.c/h
- mu1005_socket_pump.c/h
R113 native_event.h includes inbound_r5.h; inbound_r5.h includes inbound.h and mu1005_socket_pump.h. These files must be restored as a coherent dependency set, not copied individually or replaced with test stubs.
Important compatibility rule: retain X4.2's newer native_binding.c/h and bt_iap2_owner_handoff.c/h as the owner-gate/snapshot candidate; use the R113 branch's event/ingress/socket implementation only after exact include/API reconciliation. The older native_binding.c in the R113 branch does not contain X4.2's X3 gate and frozen snapshot contract, so do not replace X4.2 with it.

## 4. R113 iAP2 stream join is not full CarPlay control
r113/src/mu1005_iap2_stream.c bridges the EntryNav2 iAP2 Link core to the native inbound byte stream with a bounded FIFO and one outstanding SDK TX packet. It intentionally discards RX/control and state callbacks until an OEM consumer is proven. It proves a safe byte-stream join in source/host/QNX build context, not the full Apple identify/MFi/Wi-Fi/CarPlay state machine.
AllPlay's GPL-3.0 WirelessTrigger source in AU57x_Core_BT_Data provides a separate protocol-flow reference: Identify 0x1D00/0x1D01/0x1D02; MFi 0xAA00..0xAA05; Wi-Fi 0x5702/0x5703; CarPlay 0x4300/0x4301; transport notice 0x4E0E. It uses an external/file MFi identity interface and synthetic default accessory identity. It is not an OEM MFi provider and must not be transplanted as-is.
Potential duplication hazard: MU1005's stock ipod-drvr-iap2 has a native wifi_acc_config_info path that reads SSID/Passphrase/SecurityType/Channel and sends 0x5703 after its feature gate. Do not run AllPlay's 0x5703 sender and the OEM sender on the same iAP2 session. Select one authoritative OEM session/owner and preserve original MFi identity.

## 5. Correct MU1005 transport architecture — three different layers
1. mm-ipod physical transport plugin loader: device_transport_init loads ipod-transport-%s.so, resolves ipod_module/ipod_transport and requires ABI major 2. The factory launch currently uses -t usbdevice. The recovered USB plugin ABI is the physical USB transport; it must not be replaced or silently switched.
2. iAP2 driver components: strings such as BluetoothTransportComponent, WiFiTransportComponent and BluetoothiAP2Link are inside ipod-drvr-iap2.so. Their presence does not prove a physical plugin is instantiated or that a BlueSDK RFCOMM producer is connected.
3. Bluetooth status/config sections: the available iap2.cfg reference dump's commented [bluetooth] sections are Bluetooth Connection Status reporting, not a proven transport-enable switch. The dump is explicitly not a raw config. Do not count R99 synthetic NOT_OEM config tests as original-config validation.
Additional OEM static evidence: native WiFi feature bit 0x8000, WiFi config keys SSID/Passphrase/SecurityType/Channel, message 0x5703, and Authenticated PPS values false/true/fail. The original dump did not contain a [WiFi] section; whether the real MU1005 config enables it is unknown. The PPS default object is not bound to a wireless epoch, so stale true must not be treated as same-session MFi proof.

## 6. New immediate work gate
1. Selectively restore the complete R113 dependency set listed in section 3 into X4.3, preserving the X4.2 native_binding and R98 handoff versions. Verify every restored file against its historical SHA256 manifest and record source commit/blob SHA.
2. Build a single CI target that compiles the complete X4.2 QNX ARM32 native_binding path with real production headers and sources. Do not count the existing host stub compile or R113 owner-only QNX link as this target.
3. Add a QNX ELF/NEEDED check and QEMU smoke for this complete link. It still cannot prove the fixed addresses map correctly in the real OEM process; keep all native address execution disabled without exact image mapping and owner evidence.
4. Reconcile X4.2 owner gate, R113 service publication and R116 SUSPENDED/withdrawal semantics into one lifecycle state machine. Preserve one-shot registration, independent SDP visibility, callback quiescence, generation binding, and USB takeover withdrawal.
5. Separately trace the actual MU1005 btstack iAP2 owner/registration callback. Until that provider exists, all owner gates must fail closed and no OEM attach call may run.
6. Keep E-line SI/DIO callback source mapping separate: X3 audit still shows no production SI/DIO callback source and no same-phone Bluetooth MAC/Wi-Fi station/AirPlay session binding.

## Decision
X4.3 now treats R113/R116 as the newest tested transport/publication layer, X4.2 as the owner-gate/snapshot contract, and R103/EntryNav2 as the pinned protocol-link core. No branch is merged wholesale. Next milestone is a complete reproducible QNX ARM32 build of the combined native source dependency graph, not an installer.

NO_INSTALL / safe_to_flash=false / actual_oem_iap2_owner=false / real_mfi=false / wireless_carplay_verified=false.