# GPL-3.0 original-source reuse record

Upstream repository: https://github.com/luka-dev/mib2q-carplay-rgi
Pinned upstream commit: `91e301dfed64bdb1982c036fcf7f0eb568f0bc8a`
Upstream path: `wireless/graft/eir.c`
Upstream Git blob SHA-1: `99f42933e7c6b98af11fb387a1e68727ffe31997`
License: GPL-3.0 (original repository LICENSE; this project's LICENSE carries full GPLv3 text).

This exact 1691-byte upstream file is copied unmodified and byte-verified with
`git hash-object` in `ci/host_r102_upstream_reuse.sh`.

The surrounding `au102_eir_advertisement.[ch]` wrappers are new AU57x GPL-3.0-or-later code.
The reusable helper is **not** an OEM EIR hook and has **not** been run inside
stock MU1005 btstack. It may be invoked only once original iAP2 RFCOMM registration
and SDP publication are independently verified. No original proprietary binary,
Apple keys or firmware images are included.

The donor also contains useful but target-specific modules which are **not copied**:
- `wireless/graft/graft.c`: original MHI2Q loader addresses are NOT MU1005 addresses.
- `wireless/daemon/vendor/iap2-link`: protocol/link implementation in Rust; QNX toolchain/lifecycle integration pending.
- `wireless/integration/install_wireless_candidate.sh`: candidate lifecycle source; does not confer compatibility with AU57x.

## R103 additional exact upstream Rust iAP2 Link crate

Original and UNMODIFIED files from the same pinned upstream commit:

- `wireless/daemon/vendor/iap2-link/Cargo.toml` Git object `c0aca0a954c5bfb4b9b963c479b555bb49de39ad`
- `wireless/daemon/vendor/iap2-link/src/lib.rs` Git object `c64c8949718bdcd20e96d77b59fd70293bcc7a1e`
- `wireless/daemon/vendor/iap2-link/tests/engine.rs` Git object `2808827cef6545716bbd37b52ed6bf311da46ef2`

This is GPL-3.0 code, preserved byte-exact. New adapter under `integration/r103_iap2_bridge` uses upstream `LinkEngine`. Host Cargo workspace inherits Rust edition 2024 because the original crate declares `edition.workspace=true`. This adds no production QNX Rust toolchain. The R103 bridge has no Apple MFi, real phone pairing, or BlueSDK registration evidence.
