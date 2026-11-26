# X4.3 R99 Configuration Route Reconciliation
Date: 2026-11-26
Status: offline staging evidence only / runtime NO_GO / NO_INSTALL

## Historical result recovered from the user Library
R99_DEMO_AUDIT.json identifies an OEM_NATIVE_IAP2_CONFIG_FIRST experiment for exact platform MHI2_CN_AU57x_K1006_2/MU1005.
- Original bluetooth executable SHA256: 7d5ae31396f016f811e18f7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272.
- Original ipod-drvr-iap2.so SHA256: 42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf.
- Confirmed literal markers: bluetooth.enableIap, SERVICETYPE_IAP2, iapEnabled, BluetoothiAP2Link, BluetoothTransportComponent and WiFiTransportComponent.
- The staged experiment changed bluetooth.enableIap false -> true and added a Bluetooth section to iap2.cfg.
- Exact original and staged config hashes were recorded in the R99 audit.
- Audit explicitly says stock_runtime_iap2_registration_proven=false, stock_bt_rfcomm_transport_proven=false, real_mfi_proven=false, wireless_airplay_proven=false, vehicle_verified=false, installable=false and safe_to_flash=false.

## What this changes in the X4.3 understanding
1. There is already a historical, offline-only attempt to activate the stock configuration route. Do not redo it as a new discovery task.
2. It does not establish that the runtime creates an RFCOMM registration, a BluetoothTransportComponent instance, or a successful iPhone/MFi session.
3. R99 is not a vehicle-tested solution and must not be treated as a patch to install.
4. The config route must be reconciled with the MHI2Q reference's separate bootstrap/session-side iAP2 architecture. They may differ by platform/firmware; do not assume they are interchangeable.
5. The factory mm-ipod -t usbdevice transport remains a separate physical transport boundary. Do not change it as part of the config experiment.

## Required next analysis (no vehicle write)
- Recover the exact R99 staged config files and their parser assumptions from the original R99 package; compare line-by-line with the byte-exact original config.
- Disassemble the exact MU1005 bluetooth executable around bluetooth.enableIap and iapEnabled consumers; map which process and state machine it enables.
- Disassemble ipod-drvr-iap2.so around iap2_init, config lookup, TransportComponentIdentifier, NativeTransportComponentIdentifier, BluetoothTransportComponent and WiFiTransportComponent.
- Prove whether the driver instantiates a Bluetooth transport, how it obtains the underlying RFCOMM producer, and whether its data path is the same path used by the external MHI2Q reference.
- Confirm MFi coprocessor/identity handling remains stock and session-scoped.
- Keep original config immutable; all parser experiments use host-side copies and synthetic fixtures.

## Decision
R99 is a useful prior experiment and exact configuration evidence, but it does not close the core blocker. X4.3 should advance by static call-graph/config-parser proof, not by trying the staged files on the car.

NO_INSTALL / safe_to_flash=false / vehicle_verified=false.