# AU57x C API for reused upstream iAP2 link (GPL-3.0)
The 100% upstream core lives in src/iap2_link.cpp and src/iap2_wire.cpp, pinned to EntryNav2 1c543d2db396a79356ebffc5764a20691861ad04.
We added only au57x_iap2_capi.cpp/.h to expose it to the existing MU1005 C transport code.
Use caller-owned serialized callbacks. TX success is only a bounded queue hand-off, never BlueSDK completion. No thread spawned, no socket, no OEM binary path, no MFi provider.
This layer can replace portions of our custom iAP2 link reimplementation; it is not automatically loaded in a vehicle.
Host smoke covers real upstream detect and pre-authorization payload denial. QNX target compile must be green before even contemplating integration.
DO_NOT_INSTALL / safe_to_flash=false.
