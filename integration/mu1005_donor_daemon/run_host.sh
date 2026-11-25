#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
UP="$(realpath "${1:?pinned upstream checkout}")"
OUT="$(realpath -m "${2:?evidence directory}")"
PIN=20c94f51c0f0ae88b154c12e4f604643ba23022c
test "$(git -C "$UP" rev-parse HEAD)" = "$PIN"
git -C "$UP" diff --exit-code HEAD -- wireless/daemon
mkdir -p "$OUT/fixture"
cp "$UP/wireless/daemon/probes/dio_manager.fixture.json" "$OUT/fixture/dio_manager.json"
B="$ROOT/project/source/legacy_mu1005/bluetooth"
cc -std=c99 -O2 -Wall -Wextra -Werror -I"$B" \
 "$ROOT/integration/mu1005_donor_daemon/bridge_peer.c" \
 "$B"/{mu1005_rf_ingress,mu1005_ipc_stream,mu1005_socket_pump}.c -o "$OUT/bridge-peer"
# Host executable uses upstream UnavailableAuth. No real identities, credentials,
# receiver service, SI control socket, radio or software MFi feature.
env -i PATH="$PATH" IPL_CONFIG_DIR_DIO_MANAGER="$OUT/fixture" \
 MHI2Q_CP_MODEL=LAB-ONLY MHI2Q_CP_SERIAL=LAB-ONLY MHI2Q_CP_HARDWARE=LAB-ONLY \
 MHI2Q_CP_BT_MAC=02:00:00:00:00:01 MHI2Q_CP_DEVICE_ID=02:00:00:00:00:02 \
 MHI2Q_CP_SSID=LAB-ONLY MHI2Q_CP_PASSPHRASE=labfixturepassword \
 MHI2Q_CP_CHANNEL=11 MHI2Q_CP_IP_ADDRESS=127.0.0.1 MHI2Q_CP_AIRPLAY_PORT=5000 \
 MHI2Q_CP_RECEIVER=stock210 \
 "$UP/wireless/daemon/target/debug/mhi2q-wireless-daemon" >"$OUT/daemon.log" 2>&1 &
pid=$!
trap 'kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true' EXIT
# Check actual binary startup before client, not a simulated listening server.
for i in $(seq 1 100); do
 kill -0 "$pid"
 if grep -Fq '[bridge] listening on 127.0.0.1:57342' "$OUT/daemon.log"; then break; fi
 sleep .05
done
grep -F '[bridge] listening on 127.0.0.1:57342' "$OUT/daemon.log"
timeout 40 "$OUT/bridge-peer" | tee "$OUT/bridge.log"
grep -F MU1005_DONOR_RUST_FOUR_GENERATIONS_PASS_NO_OEM "$OUT/bridge.log"
kill -0 "$pid"
# Reuse existing rollback transaction engine, not a second installer.
bash "$ROOT/ci/host_sd_txn.sh" | tee "$OUT/recovery.log"
printf '%s\n' 'NO INSTALL / no OEM execution or MFi' "donor=$PIN" "source=$(git -C "$ROOT" rev-parse HEAD)" > "$OUT/STATUS.txt"
