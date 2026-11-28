#!/bin/sh
# Edit staged copy only, never edit installed lsd.sh directly.
set -eu
[ "$#" -eq 3 ] || exit 2
mode=$1; src=$2; out=$3
case "$mode" in add|remove) ;; *) exit 2;; esac
[ -f "$src" ] && [ ! -e "$out" ] || exit 2
anchor='BOOTCLASSPATH="$BOOTCLASSPATH:$LSD_DIR/lsd.jxe"'
entry='BOOTCLASSPATH="$BOOTCLASSPATH:/eso/hmi/lsd/jars/MusicSkinR17.jar"'
awk -v a="$anchor" -v e="$entry" '
  $0==a {ac++; ai=NR}
  $0==e {ec++; ei=NR}
  /^[[:space:]]*#/ {next}
  index($0,"MusicSkinR17.jar") && $0!=e {bad=1}
  index($0,"-Xbootclasspath:$BOOTCLASSPATH") && index($0,"-jxe:$LSD_DIR/lsd.jxe") {lc++; li=NR}
  END {exit !(ac==1 && lc==1 && ai<li && ec<=1 && (!ec || ei<ai) && !bad)}' "$src" || exit 3
trap 'rm -f "$out"' 0 1 2 15
awk -v a="$anchor" -v e="$entry" -v m="$mode" '
  $0==e {next}
  $0==a && m=="add" {print e}
  {print}' "$src" > "$out"
awk -v e="$entry" -v a="$anchor" -v m="$mode" '
 FNR==1 {f++}
 $0==e {if(f==2){n++; en=FNR};next}
 f==1 {old[++i]=$0;next}
 $0==a {an=FNR}
 {if(old[++j]!=$0)bad=1}
 END {exit !(i==j && !bad && (m=="add" ? n==1 && en+1==an : n==0))}' "$src" "$out" || exit 4
sh -n "$out" || exit 5
trap - 0 1 2 15
