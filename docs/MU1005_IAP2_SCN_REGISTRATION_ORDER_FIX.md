# MU1005 RF iAP2 registration order correction (NO_INSTALL)
The existing stock native adapter in project/source/legacy_mu1005/bluetooth/inbound.c uses
reg(&rf, &slot, 0), validates the returned slot and writes slot into SDP before publishing.
Our older r113 layer instead demanded an already-known iAP2 SCN BEFORE RF registration.
That introduced a circular precondition on a unit whose stock btstack iAP2 service is not proven.
This commit implements: preflight owner + wired idle -> register RF and obtain returned SCN ->
publish iAP SDP -> independently confirm live SCN + service -> arm R105 gate.
Any ambiguous registration outcome permanently poisons the local session. Never silently retry.
Teardown requires callback quiescence. Native backend callbacks remain UNIMPLEMENTED and must
be supplied from the exact, verified original btstack owner. This changes the contract
and is not a vehicle install candidate.
R106 metadata captures Thread_bluetoothProc / OSAPI / BlueSDK read/write threads,
but does NOT attest iAP2 RF registration. R106 OEM copy summary was 0 files.
QNX cross-link validates only the ABI layer; QEMU smoke cannot prove iPhone/MFi.
safe_to_flash=false, installable=false.
