#!/bin/sh
# Compile only core objects for integration; no install command, no car target.
set -eu
cd "$(dirname "$0")"
[ "$#" = 1 ] || { echo 'usage: sh build_qnx_example.sh NEW_OUTPUT_DIRECTORY'; exit 2; }
out=$1
mkdir "$out"
qcc -Vgcc_ntoarmv7le -std=c99 -Wall -Wextra -c sha256.c -o "$out/sha256.o"
qcc -Vgcc_ntoarmv7le -std=c99 -Wall -Wextra -c txn.c -o "$out/txn.o"
