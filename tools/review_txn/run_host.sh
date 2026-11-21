#!/bin/sh
set -eu
cd "$(dirname "$0")"
b=$(mktemp -d);trap 'rm -rf "$b"' EXIT HUP INT TERM
cc=${CC:-cc}
"$cc" -std=c99 -Wall -Wextra -Werror -pedantic ${CFLAGS:-} txn.c sha256.c fixture_cli.c supervisor_fixture.c -o "$b/txn"
"$cc" -std=c99 -Wall -Wextra -Werror -pedantic ${CFLAGS:-} sha256.c digest_cli.c -o "$b/digest"
TXN_TEST_BIN="$b/txn" TXN_DIGEST_BIN="$b/digest" python3 -B test_txn.py
