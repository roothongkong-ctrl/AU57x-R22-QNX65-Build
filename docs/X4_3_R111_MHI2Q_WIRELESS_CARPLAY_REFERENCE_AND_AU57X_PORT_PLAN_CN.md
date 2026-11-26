# X4.3 R111 — MHI2Q Wireless CarPlay Reference and AU57x Port Plan

Date: 2026-11-26
Target: Audi A6L 2017, MHI2_CN_AU57x_K1006_2 / MU1005
Status: RESEARCH_INTEGRATED / PORTABILITY_UNPROVEN / NO_INSTALL

## Executive decision

A materially stronger reference implementation has been found: congobg/MHI2Q_WiFi_CarPlay. Its 2026-10-27 status reports wireless CarPlay running on MHI2Q QC, QNX 6.5 ARMv7, firmware 5092 and initially 3638. It reports a stock AirPlay 210.81 receiver, stock iAP2 client, a btstack LD_PRELOAD RFCOMM graft, a Bluetooth iAP2 bootstrap daemon, Wi-Fi session setup, and a second iAP2 session tunneled inside AirPlay. It also reports HMI/cluster integration and USB priority. This is the closest concrete working architecture found so far.

Critical portability warning: this is MHI2Q QC / APQ8064 / 3638-5092, not the user's MHI2_CN_AU57x K1006_2 / MU1005. Do not copy its binaries, build fingerprints, addresses, hook words, HMI jar, driver changes or install pack to AU57x. Treat its source and evidence as an architecture and implementation reference only. The target's SoC, process topology, exact AirPlay receiver version, btstack build, iAP2 client path, DIO and SI integration must be mapped independently.

## 1. What the reference project establishes

Repository: https://github.com/congobg/MHI2Q_WiFi_CarPlay
Reference commit: 223b42f90bf7c15fbbbef23259da36caa9bd4994
Key docs: README.md; docs/README.md; docs/Wireless CarPlay on MHI2Q QC — Technical Reference.md

Reported working factory-like flow:

1. Bluetooth bootstrap: iPhone connects to the stock btstack. A library hook adds RFCOMM and iAP2/CarPlay SDP records, bridges bytes to a daemon, and can open the iAP2 channel from the car side if the phone does not initiate it.
2. Bootstrap daemon: performs the iAP2/MFi/Identify exchange, sends current AP SSID/security/password/channel and CarPlayStartSession only after the receiver is listening.
3. Wi-Fi: iPhone joins the MMI AP and reaches the stock AirPlay receiver in dio_manager.
4. Session-side iAP2: the car's stock iAP2 client is connected through the AirPlay event/control channels (iAPSendMessage). This is distinct from the short Bluetooth bootstrap iAP2 link.
5. HMI and lifecycle: the wireless device is offered to the HMI; dio_manager starts on demand. USB remains authoritative: the wireless instance is stopped when smartphone_integrator starts USB CarPlay. On session end, Bluetooth is restored and the next wireless attempt can start a fresh receiver.
6. Diagnostics/recovery: AP station events, watchdogs, receiver-ready gates, per-session state, and reconnect recovery are explicit parts of the design.

Reference project reports (not independently reproduced by this AU57x branch): successful Bluetooth setup, Wi-Fi join, screen/control, in-session iAP2, cluster stream, USB/wireless arbitration and a 2h30m run on its tested MHI2Q platform. Its status also lists some fixes still awaiting car confirmation. Treat each statement as upstream-reported evidence, not as AU57x validation.

## 2. Key architectural correction for AU57x

The current X4.3 experimental line focuses on a native RFCOMM inbound graft, an owner-evidence gate and fixed-address native callback binding. Keep it fail-closed and non-installable. Do not assume this is the final or only integration point.

The MHI2Q reference suggests a different and potentially more maintainable split:

- Bluetooth stack extension: hook only verified library-call seams in the original btstack process; expose RFCOMM + SDP and a local IPC byte bridge. Avoid replacing the btstack executable or patching its mapped text.
- Bootstrap daemon: own the short Bluetooth iAP2 state machine, MFi accessory authentication using the car's genuine provider, Identify, Wi-Fi credential handoff, and CarPlayStartSession.
- Stock AirPlay receiver: keep the existing receiver and its DIO integration; add only the wireless session-specific protocol adaptation that is proven necessary.
- Session-side iAP2: trace and reuse the stock iAP2 client. Do not assume the Bluetooth bootstrap transport is the same as the iAP2 transport used during the Wi-Fi CarPlay session.
- HMI/SI arbitration: one receiver owner at a time; wired USB wins; wireless starts only when the USB path is idle; reconnect after the receiver restores Bluetooth.

This architecture should be adopted as a reference design, not copied mechanically. The X4.3 native binding work is a candidate seam until the AU57x original btstack library-call boundary is proven. If a stable LD_PRELOAD-compatible call seam exists in the actual AU57x btstack, prefer the least invasive verified hook over unproven absolute-address calls. Keep both options isolated until binary evidence decides.

## 3. Portability matrix — must be proven before code port

| Area | MHI2Q reference evidence | AU57x K1006_2 / MU1005 status | Required proof |
|---|---|---|---|
| CPU / QNX ABI | QNX 6.5 ARMv7, APQ8064 | QNX/ARM target assumed from supplied artifacts; exact CPU/ABI still needs binary confirmation | readelf/ELF headers, CPU, ABI, NEEDED for stock btstack/dio_manager/driver |
| btstack | build-specific fingerprints for two MHI2Q releases | Blue SDK 4.3.1 / AV2.2.2u1 material supplied; exact running process/build mapping still needs verification | stock btstack ELF hash, process path, imports/exports, loaded shared libraries |
| RFCOMM / SDP | upstream adds RFCOMM channel and iAP2/CarPlay SDP through btstack library-call hooks | existing X4.3 native graft is experimental; original callback owner and ABI not proven | trace actual RFCOMM register/open/data/close callbacks, argument ABI, callback thread and teardown |
| Bluetooth MFi | upstream uses the car's genuine MFi provider | original MFi provider/path must be identified; no substitute identity | prove the exact MFi service/library/device and successful auth flow on target |
| WLAN/AP | MHI2Q uses uap0, Marvell AP and runtime channel query | AU57x radio/AP capabilities and actual interface/config must be inspected; do not assume same Marvell interface or channel | driver/ifconfig/AP config, runtime channel, station event source, DHCP and routing |
| AirPlay/DIO | upstream keeps stock AirPlay 210.81 in dio_manager | exact AU57x receiver version and wireless support unknown | ELF hash/version, symbols, Identify, ports, Bonjour and receiver lifecycle |
| Session iAP2 | upstream stock Cinemo iAP2 client tunnels through AirPlay event/control channels | AU57x ipod-drvr-iap2 / dio_manager path needs independent tracing | trace /dev/ipod0 and candidate endpoints, iAP2 client open/connect and AirPlay iAPSendMessage |
| HMI/SI | upstream uses a virtual HMI device and SI bridge | CN K1006_2 Java/SI/DIO integration not mapped fully | identify SI lifecycle callback and HMI device/session state interfaces |
| Cluster | upstream has MHI2Q digital cockpit integration | user has mechanical instrument cluster with centre display; existing map-display project is separate and already configured | preserve current cluster path; validate wireless session feeds the same CarPlay/DIO source |
| USB priority | upstream explicitly stops wireless dio_manager when USB smartphone_integrator starts | target requirement: preserve wired CarPlay | prove start/stop owner, process exclusivity and USB takeover/recovery |

## 4. Immediate engineering sequence — one real breakthrough at a time

### Gate A — build a target-specific binary/process map (next task)

From the user's already supplied firmware and extracted files, identify and fingerprint:
- /eso/bin/apps/btstack (actual path may differ; resolve from process/launch configuration)
- ipod-drvr-iap2.so, mss-ipodiap2.so, libiap2client.so, libirc_mmx_adapter.so
- dio_manager and its AirPlay receiver library
- smartphone_integrator, dio_manager configuration and launch scripts
- WLAN/BT driver, connection manager and AP configuration

Produce one table of SHA256, ELF class/machine/ABI, NEEDED, exports/imports, strings, process owner and launch order. No new car upload request unless a specific missing file is proven.

### Gate B — trace the actual AU57x Bluetooth seam

Use static analysis of the supplied stock btstack and libraries to find:
- RFCOMM server-channel registration and SDP record registration
- RFCOMM open/connect/data/close callbacks
- iAP2 transport selection and any runtime endpoint/path passed to the iAP2 client
- the exact library-call seam that can be safely intercepted in the stock process
- the MFi authentication provider and its ownership

Compare the X4.3 fixed-address inbound graft against the reference project's library-call hook. Choose based on actual AU57x evidence, not preference. Do not call unknown function pointers or write to stock text.

### Gate C — prove bootstrap only

A host/QNX test should demonstrate byte-preserving RFCOMM ↔ local IPC, reconnect, timeout and cleanup. On target, only after a non-invasive probe and backup, confirm the real iPhone pairing / RFCOMM / iAP2 bootstrap. Do not claim wireless CarPlay yet.

### Gate D — stock receiver Wi-Fi session

Only after Gate C, map AP runtime SSID/password/security/channel and station association, Bonjour/mDNS, AirPlay receiver readiness, CarPlayStartSession, session teardown and watchdog. Reuse stock AirPlay/DIO wherever compatible.

### Gate E — session-side iAP2 and wired coexistence

Prove the stock iAP2 client can run inside the AirPlay session. Then test USB priority, wireless stop/restart, audio routing, HMI controls, and recovery. Finally connect the wireless DIO source to the existing cluster-map output without changing its established display behavior.

## 5. What must not be done

- Do not port MHI2Q QC binaries, fixed addresses, btstack build fingerprints, hook words, driver blobs, HMI jar or install scripts to AU57x.
- Do not use a fake MFi identity, bypass MFi, or create a parallel synthetic iAP2 session that competes with the OEM owner.
- Do not send a second competing Wi-Fi credential / 0x5703 message on the same session.
- Do not replace the stock AirPlay receiver or iAP2 client until the target's existing path has been mapped.
- Do not break wired CarPlay: one DIO owner, USB wins, wireless process must yield cleanly.
- No installer, no live attach, no fixed-address execution, no vehicle changes until ABI, owner, teardown and rollback evidence pass.

## 6. Immediate outcome and next commit

The immediate next useful output is not another generic test harness. It is an AU57x binary/process map plus a source-level comparison of:
1. X4.3's current native RFCOMM inbound approach;
2. MHI2Q's btstack library-call/LD_PRELOAD RFCOMM hook;
3. the stock DIO/iAP2 session handoff.

Then implement only the first target-proven hook seam and a byte-bridge test. This directly advances the actual Bluetooth bootstrap boundary and avoids more peripheral CI work.

NO_INSTALL / safe_to_flash=false / AU57x wireless CarPlay not yet verified.
