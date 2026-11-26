# X4.3 R106 — R99 Fixture Provenance Correction and MU1005 Next Gate
Date: 2026-11-26
Status: RESEARCH_ONLY / NO_INSTALL / safe_to_flash=false

## 1. Correction: R99 was a synthetic demonstration, not an OEM config test
Repository artifacts inspected:
- R99_FINAL_STATUS.json: synthetic_demo_only=true; genuine_vehicle_bluetooth_config_provided=false; genuine_vehicle_iap2_cfg_provided=false; stock_iap2_rfcomm_runtime_verified=false; Apple MFi/AirPlay/vehicle_verified=false.
- build_r99_release.py: explicitly packages tests/fixtures/R99_DEMO_NOT_OEM_bluetooth.json and tests/fixtures/R99_DEMO_NOT_OEM_iap2.cfg, then copies them into r99_result/R99_DEMO_NOT_OEM_STAGING/.
- R99_DEMO_AUDIT.json and SHA256SUMS_R102.txt: the recorded config hashes match those NOT_OEM fixture files and their staged copies.
- The real OEM Bluetooth and ipod-drvr-iap2.so ELF hashes were pinned and checked for literal markers, but that is separate from config authenticity.
Therefore, R99's 23 passing host tests validate only synthetic parser/staging fixtures. They do NOT validate the original vehicle Bluetooth JSON schema, the original MU1005 iap2.cfg parser semantics, runtime RFCOMM registration, MFi, or wireless CarPlay. R99 must not be counted as an OEM config activation milestone.

## 2. Separate confirmed evidence from assumptions
CONFIRMED STATIC-MU1005:
- Original bluetooth executable pinned at SHA256 7d5ae31396f016f811e18f7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272; literal markers include bluetooth.enableIap, SERVICETYPE_IAP2 and iapEnabled.
- Original ipod-drvr-iap2.so pinned at SHA256 42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf; literal markers include BluetoothiAP2Link, BluetoothTransportComponent and WiFiTransportComponent.
- Original iap2.cfg dump description records [transport] name=Lightning Connector / id=1234 and a commented Bluetooth template with enable, id, name, connectstatus and mac fields.
- The driver contains transport_send_pkt / transport_recv_pkt, bt_start_updates / bt_update_recv / bt_send_info, and transport-component identifiers. Literal presence does not prove constructor execution or a working RFCOMM producer.
- The factory mm-ipod path remains -t usbdevice. Its recovered plugin ABI is a separate physical USB Transport and must not be replaced by the iAP2 Bluetooth component.
NOT PROVEN:
- The exact production Bluetooth JSON from the user's K1006_2/MU1005 app.img is not in the R99 fixture set.
- R99's staged [bluetooth] section only appended enable=1 to its demo iap2.cfg. It did not validate whether the real OEM parser also requires id/name/connectstatus/MAC or derives these fields elsewhere.
- No static call graph yet proves how the Bluetooth component obtains RFCOMM packets or which process owns the endpoint.
- No same-session MFi success, AirPlay wireless session or vehicle-level wireless CarPlay is established.

## 3. External reference: congobg MHI2Q workflow
The 2026-10-27 upstream status reports a complete wireless workflow on a different MHI2Q QC firmware: Bluetooth bootstrap iAP2, Wi-Fi credential/channel handoff, stock AirPlay receiver, session-side iAP2 carried through AirPlay event/control messages, Bluetooth release, and reconnect. This is a high-value architecture/test reference only. MU1005's libiap2client exposes iap2_raw_msg_send, but no equivalence to upstream iAPSendMessage has been proven.
Key implication: inspect the stock MU1005 DIO/iAP2/AirPlay call graph before designing a relay. Do not assume the iap2_raw_msg_send API alone is sufficient; determine its IPC message envelope, connection/session handle, transport ownership and whether the API can be used after the physical Bluetooth bootstrap link is released.

## 4. Next exact technical tasks
1. Recover the exact original Bluetooth JSON and full iap2.cfg from the user's K1006_2/MU1005 app.img or verified backup. Record path, size, SHA256, firmware provenance and extraction method. Never substitute the R99 NOT_OEM fixtures.
2. Build a read-only config inventory: active sections, commented sections, duplicate keys, parser normalization, all required Bluetooth identity fields, and cross-references to the exact driver's strings/xrefs.
3. Disassemble exact MU1005 ipod-drvr-iap2.so around iap2_init/init_features and the config parser callsites. Resolve the constructor and function table for BluetoothTransportComponent and WiFiTransportComponent; trace all callers/callees around transport_send_pkt/transport_recv_pkt and bt_start_updates/bt_update_recv.
4. Distinguish a structured PPS/status callback from raw RFCOMM data ingress. No callback may be labeled a raw producer without byte-buffer ownership, length, peer/session and send/receive evidence.
5. Inspect exact MU1005 libiap2client.so.1 and dio_manager around iap2_connect, iap2_raw_msg_send, session-created callback, AirPlay event/control handling and disconnect/teardown. Recover the API request/reply envelope and ownership model.
6. Compare the MU1005 edges against congobg's reference and joeyQuery's endpoint trace. Mark each SAME / DIFFERENT / UNKNOWN and keep evidence class tags.
7. Keep all work offline/read-only. Do not edit actual vehicle configs, mm-ipod -t, PF, startup scripts, OEM binaries, pairing state or MFi state.

## 5. Decision
R106 closes a provenance ambiguity, not a runtime feature gate. It prevents the team from treating synthetic R99 tests as genuine OEM config validation. The next useful step is exact OEM config provenance plus static parser/transport call-graph recovery. No installation package or vehicle write is authorized by this result.

NO_INSTALL / safe_to_flash=false / vehicle_verified=false.