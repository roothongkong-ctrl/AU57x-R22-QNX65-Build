#!/bin/bash
# Build inside existing QNX 8.5 image, workspace mounted at /src.
# One preload, original MIB AirPlay source unchanged, only RGI iAP2 modules.
# REVIEW ONLY: no vehicle-side installation or claim of target compatibility.
set -euo pipefail
cd /src
P=arm-unknown-nto-qnx6.5.0eabi
export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
export QNX_HOST=/opt/qnx650/host/linux/x86
export QNX_TARGET=/opt/qnx650/target/qnx6
mkdir -p evidence/obj
O=/src/evidence/obj
A=/src/upstream/thirdparty/mhi2_altscreen_carplay
M=( "$A/libaltscreen111_gen2.c" "$A/src/alt111_profile.c" "$A/src/alt111_control.c" "$A/src/alt111_video.c" "$A/src/alt111_resync.c" /src/upstream/carplay/carplay_launcher.c )
stock=()
for f in "${M[@]}"; do
 o="$O/mib-$(basename "$f" .c).o"
 "$P-gcc" -fPIC -mfloat-abi=softfp -O2 -std=gnu99 -Wall -Wextra -I"$A/include" -c "$f" -o "$o"
 stock+=("$o")
done
"$P-gcc" -shared "${stock[@]}" -Wl,-soname,libcarplay_cluster.so -lsocket -o evidence/libcarplay_cluster.baseline
# Use the original framework and RGI parser. No second AirPlay seam, coverart,
# audio, wireless, vehicle, telephony, alternate video or decoder module.
cat > "$O/rgd_only_main.c" <<'EOF'
#include "framework/hook_framework.h"
#include "routeguidance/rgd_hook.h"
const hook_module_def_t* const hook_module_table[] = { &rgd_module_def };
const size_t hook_module_table_count = sizeof(hook_module_table)/sizeof(hook_module_table[0]);
EOF
R=/src/luka/hook
rgd=()
for f in "$R/framework/logging.c" "$R/framework/state_trace.c" "$R/framework/signal_guard.c" "$R/framework/bus.c" "$R/framework/iap2_protocol.c" "$R/framework/hook_framework.c" "$R/routeguidance/rgd_tlv.c" "$R/routeguidance/rgd_hook.c" "$O/rgd_only_main.c"; do
 o="$O/rgd-$(basename "$f" .c).o"
 "$P-gcc" -fPIC -O2 -std=gnu99 -fvisibility=hidden -D__QNX__ -DHOOK_AUDIO=0 -DHOOK_WIRELESS=0 -mfpu=neon -mfloat-abi=softfp -I"$R" -c "$f" -o "$o"
 rgd+=("$o")
done
"$P-gcc" -shared "${stock[@]}" "${rgd[@]}" -Wl,-soname,libcarplay_cluster.so -lz -lsocket -lm -o evidence/libcarplay_cluster.rgd70.review
# Exact original exported names, plus only five iAP2/Cinemo seams.
names(){ "$P-nm" -D --defined-only "$1" | awk 'NF>=3 {print $3}' | sort -u; }
names evidence/libcarplay_cluster.baseline > evidence/stock-exports.txt
names evidence/libcarplay_cluster.rgd70.review > evidence/merged-exports.txt
cat > evidence/iap2-exports.txt <<'EOF'
CinemoCreateIAP
_ZN12NmeTransport4RecvER8NmeArrayIhE
_ZN12NmeTransport4SendEPKhjPj
_ZN14NmeIAP2Message6DecodeEPKhi
_ZNK14NmeIAP2Message6EncodeER8NmeArrayIhE
EOF
sort -u evidence/iap2-exports.txt -o evidence/iap2-exports.txt
if [ -n "$(comm -12 evidence/stock-exports.txt evidence/iap2-exports.txt)" ]; then
 echo "Unexpected collision with stock MIB export"; exit 1
fi
sort -u evidence/stock-exports.txt evidence/iap2-exports.txt > evidence/expected-exports.txt
diff -u evidence/expected-exports.txt evidence/merged-exports.txt
"$P-nm" -D --undefined-only evidence/libcarplay_cluster.baseline | awk '{print $NF}' | sort -u > evidence/stock-undefined.txt
"$P-nm" -D --undefined-only evidence/libcarplay_cluster.rgd70.review | awk '{print $NF}' | sort -u > evidence/merged-undefined.txt
comm -13 evidence/stock-undefined.txt evidence/merged-undefined.txt > evidence/new-undefined.txt
if grep -E '^(rgd_|hook_|bus_|AirPlay|altscreen_|coverart_|vehicle_|tel_|decoder_optimize_)' evidence/new-undefined.txt; then
 echo "Unresolved module dependency"; exit 1
fi
if "$P-nm" evidence/libcarplay_cluster.rgd70.review | grep -E 'emutls|__cxa_throw|__gxx_personality'; then exit 1; fi
"$P-readelf" -h -d evidence/libcarplay_cluster.rgd70.review > evidence/merged-elf.txt
"$P-strip" --strip-debug evidence/libcarplay_cluster.rgd70.review
(cd evidence && sha256sum libcarplay_cluster.rgd70.review > merged-SHA256SUMS.txt)
echo "PASS: one preload, unchanged MIB AirPlay exports, only five additional iAP2 exports; no on-car test."
