#!/bin/sh
# Run inside actual SDK QNX 6.5 IFS, with no vehicle paths or writes.
set -eu
PATH=/proc/boot:/bin:/usr/bin
export PATH
/proc/boot/music_crc --selftest
[ "$(/proc/boot/music_crc /proc/boot/music_r25_vector)" = cbf43926 ]
if /proc/boot/music_crc /proc/boot/does_not_exist; then exit 11; fi
music_crc_file() { /proc/boot/music_crc "$1"; }
[ "$(music_crc_file /proc/boot/music_r25_vector)" = cbf43926 ]
echo MUSICSKIN_QNX_SHELL_OK
