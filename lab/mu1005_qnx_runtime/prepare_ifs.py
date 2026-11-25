"""Adapt only two anchors in the pinned upstream QEMU launcher."""
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text()
copy = 'cp "$BIN" "$WORK/$NAME"'
manifest = 'devc-serdebug\n$NAME\nEOF'
log = 'if ! grep -q \'@@QNXRUN-START@@\' "$LOG"; then'
for a in (copy, manifest, log):
    if s.count(a) != 1:
        raise SystemExit(f"unexpected upstream launcher anchor: {a}")
s = s.replace(copy, copy + '\ncp "${QNX_TEST_NATIVE_LIB:?missing exact library}" "$WORK/libmu1005_bt_native_review.so"')
s = s.replace(manifest, 'devc-serdebug\nlibmu1005_bt_native_review.so\n$NAME\nEOF')
s = s.replace(log, '[ -z "${QEMU_KEEP_LOG:-}" ] || cp "$LOG" "$QEMU_KEEP_LOG"\n' + log)
p.write_text(s)
