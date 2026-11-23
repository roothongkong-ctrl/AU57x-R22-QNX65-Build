# AU57x Wireless CarPlay – R100 reversible factory-to-graft branch

Status: **SOURCE ONLY / NO INSTALL / NOT VEHICLE VERIFIED**.

**R100 deliverables**: Original MU1005 exact-ELF feature audit; confirmed divergences from other MHI2 firmwares; native C fail-closed factory/graft selection; new guard on QNX R98-to-R45 BlueSDK attach entry; 44-module source compile and regression tests. R99 dual config generator remains as factory branch and R97/R98 remain experimental graft branch; both still require live OEM evidence.

Chinese explanation: `docs/R100_FACTORY_TO_GRAFT_CN.md`. Private OEM files are NOT included.

Reproduce with original files on a private machine:
```sh
R100_ORIGINAL_DIR=/path/to/MU1005_original_ELFs bash ci/host_r100_route_audit.sh
python3 ci/host_r100_continuous.py
```

The result `r100_result/R100_ORIGINAL_MU1005_ROUTE_AUDIT.json` contains hashes and offsets only. It must not be treated as an installation endorsement. **Do not disable MFi, modify the original `/eso/bin/apps/btstack`, replace factory `bluetooth`, or install any review ZIP to the car.**
