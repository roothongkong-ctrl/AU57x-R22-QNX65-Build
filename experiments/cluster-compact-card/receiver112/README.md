# MIB Cluster Map v1.2.14 - R5 Type112 real transport experiment

Reuses the exact upstream v1.2.14 alt111_video.c state machine and H264 codec parser, with an independent POSIX socket ingestion component. The receiver consumes actual bytes using recv(), supports the existing 128-byte screen header and encrypted VIDEO_FRAME, independently validates avcC and the complete H264 access unit, and clears all queued frames on bad data or disconnect. A non-null per-connection AES-CTR callback is mandatory. Host tests use fake XOR encryption only to exercise this callback, not as a CarPlay substitute.

Three GitHub sanitizer rounds check real socketpair traffic, fragmentation, 1000 reconnects, negative crypto, and independent original 111 state.

NOT IMPLEMENTED: iPhone display 112 advertisement/SETUP, actual per-stream AES key derivation with stock runtime, QNX ARM compilation, second H264 decoder to RGBA, transparency or card mask, GaoDe iPhone proof, installer or rollback. As such the receiver is real socket code but still an isolated test component, NOT a vehicle-ready 112 receiver. No MIB original files changed. Do not install.
