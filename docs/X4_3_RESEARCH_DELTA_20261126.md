# X4.3 Research Delta — MHI2Q Wireless CarPlay Reference Audit
Date: 2026-11-26
Branch: x4.3-integration-plan-20261126
Status: reference audit only; no AU57x implementation or vehicle claim.

## New high-value public evidence

### A. congobg/MHI2Q_WiFi_CarPlay
Repository: https://github.com/congobg/MHI2Q_WiFi_CarPlay
Status document: https://github.com/congobg/MHI2Q_WiFi_CarPlay/blob/main/docs/README.md
Technical reference: https://github.com/congobg/MHI2Q_WiFi_CarPlay/blob/main/docs/Wireless%20CarPlay%20on%20MHI2Q%20QC%20%E2%80%94%20Technical%20Reference.md
Target declared by upstream: MHI2Q QC / Harman 2016 / QNX 6.5 ARMv7 / APQ8064 / firmware 3638 MU0917 and 5092. This is NOT the user's MHI2_CN_AU57x K1006_2 / MU1005 platform.
The upstream status page (last update 2026-10-27) reports 5092 wireless sessions with screen, controls and iAP2 in-session; Wi-Fi channel handoff; Bonjour startup; stock iAP2 client startup; Bluetooth bootstrap; instrument-cluster map stream; audio/calls/Siri; disconnect/reconnect; USB/wireless coexistence; and a 2 h 30 min session. Several subsequent fixes remain car-test-pending. Treat these as upstream-reported MHI2Q evidence, not AU57x evidence.

Key architecture extracted from upstream documentation:
1. Bluetooth is the bootstrap/control plane: discover/pair, establish an initial iAP2 link, authenticate/identify, deliver Wi-Fi credentials and initiate CarPlay.
2. Wi-Fi carries the high-bandwidth AirPlay/CarPlay session to the stock receiver.
3. A second, session-side iAP2 connection runs inside the AirPlay session using iAPSendMessage over AirPlay event/control channels.
4. Once the in-session iAP2 link is established, the phone drops its Bluetooth iAP2 link and the head unit releases Bluetooth for the CarPlay session; Bluetooth is restored at teardown.
5. Upstream implementation adds a btstack hook + daemon + pairing helper + dio_manager preload/bridge + HMI bridge; it retains stock receiver/iAP2/HMI. It handles USB and wireless with a single receiver owner; USB wins.
6. Upstream identifies timing/order bugs (Wi-Fi channel read too early, mdnsd absent, iAP2 client not started, dio_manager startup timeout, event-channel replies delayed/queue overflow, stale receiver gate, pairing identity loss, audio thread blocking). These become test cases and failure modes to investigate, not code to transplant.

Important design implication for AU57x:
- Do NOT assume Bluetooth and Wi-Fi must both carry traffic continuously. Bluetooth may only be needed for bootstrap; Wi-Fi carries the session, and Bluetooth may be released after in-session iAP2 is authenticated. Confirm this on MU1005 before implementing.
- Do NOT assume R104's iAP2 transport vtable is the same thing as the upstream Bluetooth bootstrap transport. Separate physical Bluetooth iAP2, session-side iAP2-over-AirPlay, and the USB iAP2 path.
- Add an explicit research target: determine whether MU1005's stock iAP2 client can be driven over an AirPlay session using an event/control-channel iAPSendMessage equivalent, and how its DIO receiver/session lifecycle is controlled.
- Preserve the user's currently working instrument-cluster map route. Upstream proves this behavior is technically possible on a different MHI2Q target, not that AU57x needs the same cluster code.

### B. joeyQuery/MHI2-wirelessCarPlay
Repository: https://github.com/joeyQuery/MHI2-wirelessCarPlay
Latest indexed main commit observed during this audit: 3db007ae20226adc3e1aab4ca85c6626c09178f8.
Useful established edges:
- Bluetooth active-device callback -> IapDeviceServices -> CIapBTChannel::updateiAPDevice() -> openiAPDevice() -> open64(runtime endpoint).
- Production DIO independently uses caller-supplied /dev/ipod0.
- The runtime endpoint value/owner and its relationship to DIO's resource-manager ABI remain unresolved in that repo.
- AirPlay Bonjour registration uses its interfaceName; the population source and packet/multicast interface path remain open.
- The endpoint owner and DIO handoff are still explicitly unresolved. Do not claim that the runtime endpoint equals /dev/ipod0.
Relevance: a direct match for the MU1005 R104 investigation, but firmware provenance differs (joey repo identifies MU0678-class/MHI2). Use trace methods and function boundaries only; compare every edge against MU1005 binary hashes.

### C. luka-dev/mib2q-carplay-rgi
Repository: https://github.com/luka-dev/mib2q-carplay-rgi
Latest public wireless documentation describes stock AirPlay 210.81, single SI owner, Rust daemon, Bluetooth RFCOMM graft, pairing/media bridge and iAP2/MFi/Wi-Fi bootstrap. It also explicitly lists full wireless acceptance as incomplete and tracks MainHigh control/RTCP, connection policy, native HMI audio focus and combined long-run sessions.
Relevance: keep protocol-level R103 source reuse pinned; use its test matrix and known limitations. Do not assume its MHI2Q-specific graft/binaries are compatible with MU1005.

## AU57x cross-check and branch hygiene
- R104 MU1005 ELF: iap2_init, init_features, transport_send_pkt/transport_recv_pkt and Bluetooth status/PPS functions identified. bt_update_recv is NOT proven raw RFCOMM ingress.
- R105 real vehicle snapshot: mm-ipod launched with -t usbdevice; btstack and bluetooth are separate processes; no Bluetooth iAP2 transport attach was captured; no wireless session proven.
- Therefore, the MHI2Q bootstrap + in-session iAP2-over-AirPlay architecture is a high-value hypothesis to test against MU1005, not an implementation recipe.
- GitHub compare: R103 → R143 is diverged (R143 ahead 90, behind 68) and removes a large set of earlier CI/build artifacts while adding observer/QNX review files. Do not merge branch wholesale.
- GitHub compare: R103 → owner-ingress-native is diverged (ahead 11, behind 34), adds an experimental daemon/owner-ingress integration. Audit its source and evidence before selecting individual files; do not merge wholesale.
- X4.2 report exists in the user Library snapshot but was not found at the expected path on the R143 branch; treat it as a separate patch artifact pending exact source reconciliation.

## New next tasks (ordered)
1. Obtain/inspect the full congobg technical reference and map each upstream step to an AU57x evidence row: SAME / DIFFERENT / UNKNOWN. Specifically map Bluetooth bootstrap, Wi-Fi handoff, in-session iAP2 over AirPlay, Bluetooth release, DIO lifecycle, mdnsd, HMI device state, cluster map and USB takeover.
2. Continue R104 MU1005 static analysis: resolve iap2_init config strings and component selection; recover transport object constructor/function table; trace caller/callee and PLT/GOT for transport_recv_pkt, transport_send_pkt, bt_send_info and bt_update_recv registration. Do not connect R103 LinkEngine to bt_update_recv.
3. Add a distinct research experiment for the possibility of session-side iAP2-over-AirPlay; no OEM patch until MU1005 evidence identifies the real call boundary and ownership.
4. Audit upstream licenses and provenance before any source copy. Protocol ideas and test cases can be independently implemented; do not repackage incompatible or non-commercial code.
5. Keep 2.4 GHz as the initial target. Treat 5 GHz as optional, only if the exact AU57x radio/driver/regulatory evidence proves it. Do not block wireless CarPlay on 5 GHz.
6. Preserve USB and cluster map behavior; any future test must have a rollback and must not change the working display configuration.

## Evidence labels
- [UPSTREAM-MHI2Q] reported/tested on congobg's MHI2Q target only.
- [STATIC-MU1005] static evidence from exact MU1005 ELF/config.
- [VEHICLE-AU57X] captured on the user's actual AU57x vehicle.
- [HOST] host-only fixture/test.
- [QNX] target QNX build/runtime.
Never promote one class to another without new evidence.
