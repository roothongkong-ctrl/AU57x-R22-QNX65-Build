# X4.3 AU57x Crosswalk — Proven MHI2Q Wireless Flow vs MU1005
Date: 2026-11-26
Status: analysis-only / NO_INSTALL

This document converts the strongest external end-to-end reference into testable questions for the user's exact MU1005 platform. It is not a porting instruction.

| MHI2Q reference stage | MHI2Q evidence | MU1005 evidence | AU57x status | Next proof required |
|---|---|---|---|---|
| Bluetooth pairing/discovery | congobg reports stock btstack hook, SDP/iAP2 RFCOMM and successful iPhone pairing | btstack/bluetooth processes and BlueSDK threads captured; iAP2 driver contains BluetoothTransportComponent strings; original iap2.cfg Bluetooth section is commented | PARTIAL / not active-session proof | Decode exact config parser and transport selection; identify the live callback and owner on this exact firmware |
| Bluetooth bootstrap iAP2/MFi | upstream wcpd reports authentication/identification and Wi-Fi handoff | iAP2 binary has Bluetooth status, transport abstraction and MFi-related APIs; no live BT iAP2 owner or authentication trace | STATIC ONLY | Map iap2.cfg feature/component creation, real RFCOMM producer and stock MFi success callback |
| Wi-Fi handoff | upstream sends Wi-Fi credentials and reads actual running AP channel after AP is up | uap0 2.4 GHz AP and a station association observed on vehicle; iAP2 strings include RequestWiFiInformation / AccessoryWiFiConfigurationInformation / WiFi SSID/BSSID | PARTIAL | Prove phone identity correlation, exact channel/credential handoff, IP lease, route and PF rules |
| AirPlay/Bonjour session | upstream reports mdnsd, AirPlay receiver, encrypted control/media session | wired CarPlay receiver works; libairplay and mdnsd infrastructure exist; current factory DIO config points direct link to carplay0 | PARTIAL (wired only) | Recover AirPlay interfaceName writer and prove receiver can advertise/bind on uap0 without disrupting carplay0 |
| Session-side iAP2 over AirPlay | upstream reports stock iAP2 client (Cinemo) starts a second iAP2 session inside AirPlay; packets carried as iAPSendMessage over event/control channels | libiap2client.so.1 and iap2_raw_msg_send exist; DIO imports iap2_connect; no evidence of iAPSendMessage support or equivalent path in MU1005 | UNKNOWN / high-value target | Inspect exact MU1005 libairplay + dio_manager + libiap2client call graph and message ABI; find whether same capability exists or requires a compatible bridge |
| Bluetooth released during CarPlay | upstream reports Bluetooth iAP2 drops and Bluetooth is released after in-session iAP2 is authenticated | no corresponding MU1005 trace | UNKNOWN | Determine stock Bluetooth/audio profile policy and teardown order; verify phone calls/audio routing before considering any release |
| DIO lifecycle | upstream starts dio_manager on demand for wireless and stops it on teardown; single owner | wired SI/DIO/AirPlay chain verified; USB starts mm-ipod with -t usbdevice | WIRED PATH PROVEN; wireless lifecycle unknown | Determine whether stock SI starts/stops DIO for a wireless offer; prove one receiver owner and rollback barrier |
| HMI integration | upstream HMI virtual device and AVAILABLE/CONNECTED/STARTED state machine verified | smartphone_integrator and DIO exist; original MMI CarPlay works wired | WIRED PATH PROVEN; wireless offer unknown | Recover production HMI admission/state interface; avoid competing UI/device state |
| Cluster map | upstream reports instrument-cluster map stream | user already has cluster map display working in current CarPlay setup | EXISTING USER PATH TO PRESERVE | Capture baseline and confirm same-session map remains after wireless session; no display redesign |
| USB/wireless priority | upstream reports one receiver owner, USB wins, and wireless resumes after USB teardown | original USB path is -t usbdevice; user requires wired priority | REQUIREMENT DEFINED; implementation unproven | Design arbitration without replacing -t usbdevice; verify restore barrier before wireless resumes |
| Reconnect/retry | upstream reports reconnect, two phones and long-run stability; several fixes were learned | user-approved behavior: last phone first, bounded retry, prompt after retry limit | REQUIREMENT DEFINED; implementation unproven | Derive timers/retries from measured AU57x events; preserve pairing identities |

## Key architecture insight
The reference does not keep Bluetooth and Wi-Fi as equal continuous media links. Bluetooth is used to bootstrap and authenticate, then Wi-Fi carries the main session. The reference additionally moves the car's session-side iAP2 traffic into the AirPlay event/control channel, after which the Bluetooth iAP2 link can be released. This is a reference design only. MU1005 must prove whether its stock DIO/iAP2/AirPlay interfaces support the same model.

## Critical distinction: three different transport boundaries
1. mm-ipod physical transport plugin: the user's recovered USB plugin ABI (ipod_module / ipod_transport) and -t usbdevice; this is the factory wired transport. Do not replace it.
2. iAP2 Bluetooth component: strings in ipod-drvr-iap2.so include BluetoothTransportComponent, BluetoothiAP2Link, Bluetooth MAC/profile and Wi-Fi information fields; the stock iap2.cfg snapshot has only [transport] name=Lightning Connector, id=1234 and a commented [bluetooth] block. Exact parser/selection behavior and runtime activation are not proven.
3. session-side iAP2 over AirPlay: a second iAP2 exchange carried inside the established Wi-Fi/AirPlay session in the MHI2Q reference. MU1005's iap2_raw_msg_send API is a candidate boundary to inspect, not proof that it supports the same message relay.

These boundaries must not be conflated. Do not infer that changing mm-ipod -t selects the Bluetooth component, or that uncommenting [bluetooth] alone enables wireless CarPlay.

## Highest-value next research batch
1. Parse the exact MU1005 iap2.cfg from the 70-version dump and identify all active/disabled component names, transport IDs, Bluetooth status keys and Wi-Fi handoff fields. Keep original unchanged.
2. Use exact pinned MU1005 ipod-drvr-iap2.so and libiap2client.so.1 to trace the constructor and selection logic for BluetoothTransportComponent / WiFiTransportComponent; resolve the transport function table and the transport_recv_pkt producer.
3. Inspect exact MU1005 dio_manager for iap2_connect callsites, iap2_raw_msg_send usage, session-created callback, AirPlay event/control channel message dispatch and interfaceName property assignment.
4. Inspect exact MU1005 libairplay.so for event/control channel API and whether iAPSendMessage-equivalent dispatch exists.
5. Compare these call graphs with the congobg and joey reference traces. Mark each edge STATIC-MU1005 / VEHICLE-AU57X / UPSTREAM-MHI2Q / HOST / UNKNOWN.
6. Keep all experiments offline/read-only until a reversible runtime design is proven. Do not modify iap2.cfg, -t usbdevice, PF, startup scripts or OEM binaries on the car in this research batch.

## Product behavior remains fixed
- Bluetooth + Wi-Fi setup, no internet required.
- Bluetooth may be released after bootstrap if the verified stock architecture requires it; automatic reconnection at next drive remains.
- Last successful iPhone first; other authorized phones only if unavailable.
- Leave range: clean teardown; return: reconnect without pairing.
- Bounded retries then manual prompt.
- USB wins; after full stock teardown, wireless may resume.
- Preserve the current MMI CarPlay screen and instrument-cluster map, audio and controls.

NO_INSTALL / safe_to_flash=false / vehicle_verified=false.