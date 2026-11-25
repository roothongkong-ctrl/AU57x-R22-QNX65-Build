#!/bin/bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
r="$(cd "${1:?R29 expanded source directory required}" && pwd)"
b="$root/project/source/legacy_mu1005/bluetooth"
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
cc -std=c99 -Wall -Wextra -Werror -DWCP_TCP_HOST_TEST -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie -I"$b" -I"$r" -I"$here" "$b"/{mu1005_rf_ingress,mu1005_ipc_stream,mu1005_socket_pump}.c "$r"/{wcp_oem_graft_io,wcp_graft_frames,wcp_stream_bridge}.c "$here"/{tcp_adopt,test_tcp}.c -o "$tmp/r29tcp"
for n in 1 2 3; do timeout 15 "$tmp/r29tcp";done
