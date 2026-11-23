#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
  project/source/legacy_mu1005/bluetooth/bt_route_gate.c tests/test_r100_route_gate.c \
  -o /tmp/au57x_r100_route_gate_$$
trap 'rm -f /tmp/au57x_r100_route_gate_$$' EXIT INT TERM
/tmp/au57x_r100_route_gate_$$
