# AU57x R24 — Bluetooth/iAP2 normalized ingress (REVIEW ONLY)

Target 2017 Audi A6L, MHI2_CN_AU57x_K1006_2 / MU1005.

R23 QNX 6.5 ARMv7 dynamically loaded 16 C modules. R24 adds **two normalized RFCOMM/IPC modules** (18 C modules total). The extended QNX QEMU test checks generation, fragmented packets, MTU 2/2/1, async completion, stale replay rejection, and fail-closed disconnect.

**Do not misrepresent these as real BlueSDK or Apple iAP2 endpoints.** OEM callback owner, iAP2 endpoint, SI wireless admission, MFi, Wi-Fi association and DIO media are NOT wired or certified. No firmware modification, installer, release, radio access or MFi bypass.

See docs/STEP04_MU1005_OEM_CALLMAP.md for verified OEM evidence and docs/STEPS05_TO_12_ACCEPTANCE_CN.md for the separate 05–12 acceptance ledger. QNX SDK redistributability must be verified.
