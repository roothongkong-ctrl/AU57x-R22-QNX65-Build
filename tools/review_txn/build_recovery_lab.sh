#!/bin/sh
# QNX disposable emulator lab, no OEM service or production installation.
set -eu
cd "$(dirname "$0")"
[ "$#" = 1 ] || { echo 'usage: sh build_recovery_lab.sh NEW_OUTPUT_DIR' >&2; exit 2; }
cc=${QNX_CC:-arm-unknown-nto-qnx6.5.0eabi-gcc}
mkdir "$1"
"$cc" -std=c99 -D_DEFAULT_SOURCE -D_QNX_SOURCE -Wall -Wextra -Werror txn.c sha256.c supervisor_fixture.c txn_recovery_lab.c -o "$1/txn_recovery_lab"
readelf -h -l -d "$1/txn_recovery_lab" > "$1/elf.txt"
grep -Eq 'Machine:.*ARM' "$1/elf.txt"
sha256sum "$1/txn_recovery_lab" > "$1/sha256.txt"
