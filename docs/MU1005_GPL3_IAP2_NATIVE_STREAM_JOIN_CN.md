# MU1005 native RFCOMM → borrowed GPL3 iAP2 link data path
2026-11-24. Source only, NO_INSTALL.
This adapter is the **first concrete byte-stream join** between real MU1005 native inbound_send (BlueSDK queue semantics) and the upstream EntryNav2 iAP2 Link C API. It is not a production owner registration nor authentication implementation.

Serialization contract:
1. OEM proves exact image/thread, initiates RFCOMM and SDP on OEM owner, validates paired peer and original MFi hardware with R113.
2. OEM inbound_event(code=2) returns, leaves Inbound.state=IN_LINK; only then call mu1005_iap2_stream_on_open(), which starts the upstream link Detect sequence.
3. Link ByteSink enqueues bytes into 32768-byte FIFO. Pump issues at most ONE inbound_send using OEM live MTU capped at MAX_DATA=5035, then waits for event 6 (TX completed), so it does not overwrite Inbound.packet.
4. When OEM handles event 5 (RX), it may call on_bytes to feed upstream iAP2 without sending reentrantly; pump after event callback returns.
5. After real inbound_event(code=6) clears busy, pump one new chunk; repeat on later completions.
6. On USB takeover / owner revocation / generation mismatch, stop the link, drop pending bytes, and leave native service withdrawal to R113 owner.
7. Upstream control payload callback is intentionally NOOP: do **not** pass unverified Identify/MFi or raw data into existing OEM DIO.
8. No BT RFCOMM registration, AirPlay, auth, or installer is activated by this module.

Critical: No proof that MU1005 original btstack can invoke these callbacks yet; only OEM owner can bind once verified. The original BlueSDK send return 2 means queued, not transmitted. The FIFO is bounded and a full FIFO rejects further link writes.
