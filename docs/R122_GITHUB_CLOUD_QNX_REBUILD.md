# R122 GitHub Cloud QNX65 ARMv7 rebuild

This documentation-only commit intentionally triggers the existing
`.github/workflows/qnx65-compile.yml` workflow on
`mu1005-wireless-integration-20261125`.

The workflow performs:
- source-only preflight (rejects proprietary binaries/keys);
- Bluetooth/RF/IPC/socket host tests;
- QNX 6.5 ARMv7 compile using the public qnx65-armv7-toolchain container;
- strict ELF dependency/ABI gate;
- QEMU QNX runtime dlopen smoke test with no OEM calls;
- source-only logs/artifact upload.

This is a rebuild of the repository's pinned R52/R24 research source set,
not a claim that the separately generated host-only R118-R122 adapter ZIP
has been ported into this repository. The workflow's exact source-count
and ABI gates remain authoritative.

Safety: REVIEW ONLY / NO INSTALL. No OEM binaries, Apple MFi credentials,
vehicle writes, or real iPhone session are included. A successful QNX/QEMU
run proves compilation and isolated loader behavior only, not wireless
CarPlay functionality on MU1005.
