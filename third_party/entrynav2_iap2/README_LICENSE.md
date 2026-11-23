# Upstream reusable iAP2 wire and reliable-link modules (SOURCE ONLY)
Source: https://github.com/oguzkarcan/EntryNav2-CarPlay
Pinned upstream commit: 1c543d2db396a79356ebffc5764a20691861ad04
Files copied **unmodified** into this directory:
- include/carplay/iap2_link.hpp -> third_party/entrynav2_iap2/include/carplay/iap2_link.hpp
- src/core/iap2_link.cpp -> third_party/entrynav2_iap2/src/iap2_link.cpp
- include/carplay/iap2_wire.hpp -> third_party/entrynav2_iap2/include/carplay/iap2_wire.hpp
- src/core/iap2_wire.cpp -> third_party/entrynav2_iap2/src/iap2_wire.cpp
- tests/test_iap2_link.cpp -> third_party/entrynav2_iap2/tests/test_iap2_link.cpp
- tests/test_support.hpp -> third_party/entrynav2_iap2/tests/test_support.hpp
Original: GPL-3.0; link and wire files were derived by upstream from DiPlay GPL-3.0, retain all original copyright/attribution headers.
The encompassing AU57x repository is GPL-3.0.
Scope: transport-neutral iAP2 link framing/retry/detect/SYN plus iAP2 CSM codec, reused from a working source tree rather than reimplemented.
No upstream BMW HMI, Wi-Fi driver or car install routines are imported.
This does NOT prove original Audi MU1005 btstack owner hooks, iPhone MFi success, or car support.
The native MU1005 Bluetooth RF bridge is C; this C++ library is currently a review-only *candidate*, not in any car auto-start path.
Do not redistribute without GPL3 source/license notices. safe_to_flash=false, NO_INSTALL.
