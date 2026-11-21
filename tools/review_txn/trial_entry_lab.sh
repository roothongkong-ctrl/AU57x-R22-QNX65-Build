#!/bin/sh
# Lab only. No MIB launcher, no production mount/remount or service action.
set -eu
[ "$#" = 3 ] || { echo 'usage: trial_entry_lab.sh FIXTURE_BIN LAB_ROOT {install|status|recover|restore|accept-test-only}' >&2; exit 2; }
bin=$1
root=$2
case "$root" in /tmp/mu1005-c-txn-*) ;; *) echo 'not a lab root' >&2; exit 2;; esac
case "$3" in
 install) action=trial;;
 status|recover|restore) action=$3;;
 accept-test-only) action=confirm;;
 *) echo 'unknown lab action' >&2; exit 2;;
esac
exec "$bin" "$root" "$action"
