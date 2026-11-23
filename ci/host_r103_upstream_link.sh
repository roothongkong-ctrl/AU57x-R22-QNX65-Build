#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
root=third_party/luka-dev/mib2q-carplay-rgi/wireless/daemon/vendor/iap2-link
check() { actual="$(git hash-object "$root/$1")"; [ "$actual" = "$2" ] || { echo "Upstream $1 hash mismatch: $actual" >&2; exit 2; }; }
check Cargo.toml c0aca0a954c5bfb4b9b963c479b555bb49de39ad
check src/lib.rs c64c8949718bdcd20e96d77b59fd70293bcc7a1e
check tests/engine.rs 2808827cef6545716bbd37b52ed6bf311da46ef2
cargo test --offline --workspace --all-targets
printf 'R103_UPSTREAM_ORIGINAL_LINK_AND_AU57X_BRIDGE_HOST_TESTS_PASS\n'
