# AU57x / MU1005 wireless CarPlay — R32 public cloud research (NO INSTALL)

**Research-only: Not a CarPlay wireless implementation validated on the vehicle.**
2017 Audi A6L C7, MHI2_CN_AU57x_K1006_2 / MU1005, QNX 6.5 ARMv7.

Four cloud-deliverable scoped tasks (01, 02 static-only, 03 research-build-only, 10 offline-CI-only) are complete; eight total project steps remain. R24 compiled 19 C modules with strict native ELF gate and QNX QEMU dlopen. R26–R32 added explicit 12-step evidence ledger, model/authorization tests, read-only SD ZIP safety gate, deterministic CSM fuzz, and sample-pinned original Bluetooth/mm-ipod static research. These do NOT prove BlueSDK owner, genuine MFi, Wi-Fi radio, Apple identity, DIO/AirPlay media or safe installation.

Read [云端研发审查说明](docs/CLOUD_README_CN.md), [12-step evidence ledger](docs/CLOUD12_LEDGER.json), and GitHub Actions [cloud review gates](.github/workflows/cloud12-review.yml). Every passing run can publish a source-only ZIP and SHA256; no vehicle installer.


### R41 Step 04 (research only)
Stock MU1005 `libairplay.so` certificate/signature functions are now used through an owner-attested `dlsym` API, and iAP2 CSM messages AA00–AA05 have an ordered, fail-closed handoff to the wireless control gate. **No live OEM BlueSDK to iAP2 session or actual iPhone/MFi success is proven.** See `docs/R41_STOCK_MFI_INTEGRATION_CN.md`.
