#!/bin/sh
set -eu
root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
mkdir -p "$root/cloud12_result"
for mode in 0 1; do
  cc -std=c99 -Wall -Wextra -Werror -DAU57X_FAST_CONNECT_EXPERIMENT=$mode \
    -I"$root/project/source/legacy_mu1005/bluetooth" \
    "$root/tests/test_rf_arg_experiment.c" \
    "$root/project/source/legacy_mu1005/bluetooth/rf_arg_experiment.c" \
    -o "$root/cloud12_result/r91_rf_arg_$mode"
  "$root/cloud12_result/r91_rf_arg_$mode"
done
