#!/bin/sh
# R105.119 exact OEM configuration-only experiment. REVIEW / NO INSTALL.
# No mount, reboot, process kill, RF injection or automatic path selection.
set -eu
umask 077
ORIGINAL=56c14940455215233f9ead5a8cd3e3a9dc984f372b0d7421ba26ff8e8f579470
CANDIDATE=75671e53af6dddcba7df62f26e4e1e6ec62e1097d92b836f1df9c184742249b5
die(){ echo "FIRST_LINK_STOP: $*" >&2; exit 2; }
digest(){
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    elif command -v openssl >/dev/null 2>&1; then
        openssl dgst -sha256 "$1" | awk '{print $NF}'
    else die "no SHA256 utility"; fi
}
verify(){ [ "$(digest "$1")" = "$2" ] || die "unexpected SHA256: $1"; }
[ "$#" -eq 4 ] || die "usage: first_link_config.sh apply|restore TARGET NEW_OR_EXISTING_BACKUP_DIR CONFIRM_EXPERIMENT"
action=$1; target=$2; backup=$3
[ "$4" = CONFIRM_EXPERIMENT ] || die "explicit experiment confirmation required"
case "$target" in /*/connectivity.json) ;; *) die "explicit absolute connectivity.json path required";; esac
case "$backup" in /*) ;; *) die "absolute backup directory required";; esac
[ -f "$target" ] && [ ! -L "$target" ] || die "target not regular file"
dir=$(dirname "$target")
canonical=$(cd "$dir" && pwd -P)
[ "$target" = "$canonical/connectivity.json" ] || die "canonical target required; resolve real path first"
[ -d "$(dirname "$backup")" ] || die "backup parent missing"
[ ! -L "$backup" ] || die "backup directory symlink refused"
tmp=
cleanup(){ if [ -n "$tmp" ]; then rm -f "$tmp"; fi; }
trap cleanup EXIT
trap 'exit 2' HUP INT TERM
case "$action" in
apply)
    verify "$target" "$ORIGINAL"
    mkdir -m 700 "$backup" || die "new backup directory required"
    cp -p "$target" "$backup/original.json"
    verify "$backup/original.json" "$ORIGINAL"
    printf '%s\n' "$target" > "$backup/target.txt"
    tmp=$(mktemp "$dir/.au57x-first-link.XXXXXXXX")
    cp -p "$target" "$tmp"
    sed '216s/false/true/' "$target" > "$tmp"
    verify "$tmp" "$CANDIDATE"
    verify "$target" "$ORIGINAL"
    sync
    mv -f "$tmp" "$target"
    tmp=
    sync
    verify "$target" "$CANDIDATE"
    echo "FIRST_LINK_CONFIG_APPLIED_ONLY_NOT_CONNECTION"
    ;;
restore)
    [ -d "$backup" ] && [ -f "$backup/original.json" ] && [ ! -L "$backup/original.json" ] && [ -f "$backup/target.txt" ] && [ ! -L "$backup/target.txt" ] || die "backup incomplete"
    [ "$(cat "$backup/target.txt")" = "$target" ] || die "backup target mismatch"
    verify "$backup/original.json" "$ORIGINAL"
    if [ "$(digest "$target")" = "$ORIGINAL" ]; then
        echo "FIRST_LINK_ALREADY_ORIGINAL"; exit 0
    fi
    verify "$target" "$CANDIDATE"
    tmp=$(mktemp "$dir/.au57x-first-link.XXXXXXXX")
    cp -p "$backup/original.json" "$tmp"
    verify "$tmp" "$ORIGINAL"
    verify "$target" "$CANDIDATE"
    sync
    mv -f "$tmp" "$target"
    tmp=
    sync
    verify "$target" "$ORIGINAL"
    echo "FIRST_LINK_CONFIG_RESTORED_BYTE_EXACT"
    ;;
*) die "unknown action";;
esac
