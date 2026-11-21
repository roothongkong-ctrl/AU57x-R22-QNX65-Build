# AU57x R22 — GitHub Actions QNX 6.5 ARMv7 native compile (REVIEW ONLY)

For 2017 Audi A6L MHI2_CN_AU57x_K1006_2, **source/build experiment only**.

- Public repository contains **only C source, headers, compiler scripts and offline ELF gate**. No Audi OEM binaries or QNX SDK.
- `.github/workflows/qnx65-compile.yml` runs **on push to main**, or click **Actions → AU57x R22 QNX6.5 ARMv7 → Run workflow**.
- It clones [luka-dev/qnx65-armv7-toolchain](https://github.com/luka-dev/qnx65-armv7-toolchain), builds its Docker image, then compiles 13 C files and attempts to link a QNX 6.5 ARMv7 `.so`.
- Download artifact `AU57x-R22-QNX65-review-only` from the completed Actions run. It includes `qnx_build_result/logs/native_build.log`, `toolchain_docker_build.log`, and `qnx_elf_gate.json` when steps reached those points.
- Toolchain's bundled QNX sysroot may be subject to third-party **license restrictions**; publicly available is not equivalent to free redistribution or use rights. Confirm before running. No toolchain image or sysroot is published by this workflow.
- No release, no deployment, no flash: even a successful QNX link **does not** establish wireless iAP2/AirPlay integration, MFi authorization or vehicle runtime compatibility.
- Existing source has **no explicit source license** in this package. Do not assume the code grants downstream redistribution rights merely because you make the repository public. Verify provenance first.

## Public repository setup
1. Create an empty public GitHub repo named `AU57x-R22-QNX65-Build` with branch `main`.
2. Upload the **contents** of this folder, including hidden `.github/` (not the ZIP itself).
3. Open **Actions**. If GitHub asks to enable Actions, choose to enable the workflow. A push to `main` should start a job; otherwise use **Run workflow**.
4. Open job log and **Artifacts**. Send the resulting ZIP/logs back for review and next fixes.

The linked GitHub account used in chat currently has no writable repository and the provided connector lacks a create-repository/dispatch-workflow operation. Thus this archive does not claim a cloud run happened.
