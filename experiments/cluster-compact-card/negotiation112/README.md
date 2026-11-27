# R6 Type112 /info + SETUP/TEARDOWN transport negotiation lab

Based on the official MIB Cluster Map v1.2.14 source and exact uploaded SD ZIP, SHA256
7450ac0761d97a8de05e74c961c9e44e619c279fb2faa5e7683098de115485a1.

This is a **real, isolated POSIX TCP listener** with a display-112 descriptor, a
SETUP reply containing the bound dataPort and streamConnectionID, a Type112
receiver supplied by R5, the unmodified upstream H264 parser, and partial
TEARDOWN / re-SETUP. It checks 110/111 isolation. It accepts only an explicitly
opted-in, all-proofs-ready policy, validates distinct map/card UUIDs and requires
a non-null decrypt callback.

## What it does NOT prove
The six proof bits are **synthetic in these tests**. There is no real iPhone
CarPlay handshake, real Apple /info CFLite dictionary serializer, actual
AirPlay Key SHA512 per-connection derivation, real AES in the lab (the host
test uses an XOR stub), QNX ARM binary, decoder to RGBA, or verified AMap
transparent original-card image. The test listener uses 127.0.0.1 and is NOT
a real CarPlay-link listener. It must not be advertised to a real iPhone or
installed in the vehicle. This code is NOT wired into dio_manager or the
original AirPlayCopyServerInfo/SessionSetup callbacks.

## Next integration gate
Build a separate, QNX-compatible second receiver with exact per-session AES
lifecycle, and integrate it with the real CFLite /info + SETUP+ TEARDOWN
adapter *only after* confirming an iPhone/GaoDe actually offers third-screen
type 112. Instrument output cannot be modified until two live streams and
card alpha/mask are proven. Keep upstream original map 111 and console 110.

All upstream original source files and the user's v1.2.14 installer remain untouched.
