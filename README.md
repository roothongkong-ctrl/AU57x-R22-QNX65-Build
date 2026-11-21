# AU57x R23 — SI-bound AirPlay receiver native integration (REVIEW ONLY)

For **2017 Audi A6L MHI2_CN_AU57x_K1006_2 / MU1005**.

## Real changes from R22

- Restored **three original R28 C implementation files** recovered from the user's own R29/R28 source archive: `cprx_service.c` (SI-bound CPRX v3 local receiver service), `semantic.c` (bounded CSM/semantic stream) and `wifi_interface.c` (actual IPv4 interface selection). The existing matching three public headers were already present in R22.
- R23 builds **16 C source modules** into one QNX 6.5 ARMv7 `.so`. It explicitly links **libc.so.3** and **libsocket.so.3**, and checks ELF EABI5 against the MU1005 baseline.
- The GitHub Actions QNX QEMU loader test now looks up all three new exported service entry points and executes safe validation functions. It **never** opens the real AirPlay receiver, bypasses MFi, or touches a vehicle.
- The original R28 source code was transferred byte-for-byte; see git blob IDs and source archive fingerprints in the session notes.

## Scope / limitations

This is **not wireless CarPlay yet**. All native OEM ownership and authorization callbacks remain unimplemented in the public repo. The SI owner must actually approve an epoch, peer identity, DIO lifetime, Apple authentication and native media. The source does not provide a production SI hook, an RF driver, an installable patch or a standalone DIO replacement. A QEMU pass proves the modules link and load on QNX, **not** actual CarPlay sessions.

Do not flash or install these artifacts. Keep the stock wired CarPlay working and protect the vehicle with full backup/restore before any later live validation. The publicly accessible toolchain may contain separately licensed QNX components; do not assume its SDK permits all redistribution/use.

## Run CI

Push to `main`, or GitHub Actions → AU57x R23 QNX6.5 ARMv7 → Run workflow. Inspect artifact **AU57x-R23-QNX65-review-only** for `native_build.log`, `qnx_elf_gate.json`, `qnx_runtime_smoke.log` and QNX boot console. The repository contains no Audi OEM binaries or firmware.
