# AU57x / MU1005 wireless CarPlay — R77 source research (NO INSTALL)

**Research-only: Not a CarPlay wireless implementation validated on the vehicle.**
2017 Audi A6L C7, MHI2_CN_AU57x_K1006_2 / MU1005, QNX 6.5 ARMv7.

Five cloud-scoped ledger tasks (01, 02, 03, 04, 10) are DONE; seven other ledger tasks are incomplete, and eight items still require functional/vehicle acceptance (including step 04). R24 compiled 19 C modules with strict native ELF gate and QNX QEMU dlopen. R26–R32 added explicit 12-step evidence ledger, model/authorization tests, read-only SD ZIP safety gate, deterministic CSM fuzz, and sample-pinned original Bluetooth/mm-ipod static research. These do NOT prove BlueSDK owner, genuine MFi, Wi-Fi radio, Apple identity, DIO/AirPlay media or safe installation.

Read [云端研发审查说明](docs/CLOUD_README_CN.md), [12-step evidence ledger](docs/CLOUD12_LEDGER.json), and GitHub Actions [cloud review gates](.github/workflows/cloud12-review.yml). Every passing run can publish a source-only ZIP and SHA256; no vehicle installer.


### R41 Step 04 (research only)
Stock MU1005 `libairplay.so` certificate/signature functions are now used through an owner-attested `dlsym` API, and iAP2 CSM messages AA00–AA05 have an ordered, fail-closed handoff to the wireless control gate. **No live OEM BlueSDK to iAP2 session or actual iPhone/MFi success is proven.** See `docs/R41_STOCK_MFI_INTEGRATION_CN.md`.

### R45 pinned native MU1005 BlueSDK interface (NO INSTALL)
Four original R14 ARM/QNX source modules are restored and compiled in the research build: owner-gated RFCOMM registration + SDP records + event RX/TX and teardown. Private OEM `btstack` SHA-256 and 40/40 executable words were independently rechecked. Neither true OEM process ownership nor iPhone authentication is proven. See `docs/R45_NATIVE_BLUESDK_MU1005_CN.md`.

### R50 cloud integration 04–11

- Scoped Step 04 **cloud-only DONE**: R45 30-module QNX ARMv7/QEMU success run 44955933559, zero physical MFi claims.
- New `cp_step04_tcp_bridge` joins R14 57342 normalized RFCOMM socket byte frames to R44 iAP2/MFi (host test).
- `cp_cloud_pipeline` composes verified Wi-Fi bands, OEM AirPlay server listener, Bonjour identity, SI owner, inner iAP2, 4-channel media proof and wired takeover; **callbacks must be from real OEM owner in production**.
- `cp_cloud_media` routes bounded video/audio/MMI/steering messages to a caller-provided real native sink; no decoder or hardware access is provided.
- `tools/review_txn` restores the original user-supplied native transaction/recovery source and its 67 host tests. No production installer, OEM file paths or SD flash image.
- See `docs/R50_CLOUD_04_TO_11_CN.md`. All artifacts remain **review-only, NO_INSTALL**.

### R71 original SI receiver/media witness (review only)

`cp_cloud_bind_rx_service` connects the existing SI-bound CPRX service to the
same original DIO AirPlay owner and requires live receiver/session/media
observations before reporting audio/video active. Native media flows through
the OEM AirPlay decoder, not the generic test-byte router; the router rejects
native mode. Owner teardown retains original resources until native restore
completes. All observed media status is mocked in CI, not car-verified.
See `docs/R71_NATIVE_SI_MEDIA_CN.md`.

### R72 — one SI owner controls the native receiver (NO INSTALL)

R72 eliminates competing native AirPlay starts. SI CPRX v3 must be leased
**before** its first START; the original-owner periodic tick now services its
real loopback START request and waits for a confirmed reply before the CarPlay
pipeline may consider the receiver ready. Invalid epoch/session are rejected,
while the OEM restoration barrier remains mandatory. See
`docs/R72_SINGLE_SI_RECEIVER_OWNER_CN.md`. Host socket tests are not vehicle
proof; real SI/DIO callbacks are still required.

### R73 native SI owner loop + wired recovery (NO INSTALL)

The single SI CPRX owner tick now advances independently verified inner iAP2
and native media states (0/4/5 wait, 7 plus four media/HMI witnesses active),
re-checks streaming each tick, and retries incomplete OEM restoration on
repeated wired takeover calls. Host/OEM fixtures are **not** iPhone or car proof.
See `docs/R73_SI_OWNER_TICK_WIRED_RESTORE_CN.md`.

### R74 — bind inner MFi and four media/control witnesses to one SI session (NO INSTALL)

Native CPRX v3 session authorization now requires **session-scoped** original
SI callbacks for inner iAP2/MFi and video, audio, MMI and steering. A previous
phone's epoch-only success flags cannot authorize a new SI session. Runtime
checks repeat on each owner tick; dropping any callback makes MEDIA_ACTIVE
false immediately. CI uses independent host fixtures, not car callbacks.
See `docs/R74_SI_SESSION_SCOPED_PROOFS_CN.md`.

### R75 — local SI client admission for CPRX v3 (NO INSTALL)

The original receiver control listener previously accepted any local TCP
connection that knew the generation and session numbers. R75 requires a
separate, owner-supplied **OS peer identity verification** callback before
reading a v3 request, and repeats admission while the client is active.
An unauthorized process cannot issue START/STOP or media metadata commands.
No QNX peer-identity mechanism is guessed: the integration must provide a
verified OEM implementation; host fixtures only check admission behavior.
See `docs/R75_SI_IPC_CLIENT_ADMISSION_CN.md`.


### R76 SI v3 Unix peer credential migration (NO INSTALL)

R76 shuts off the old SI v3 TCP control entrypoints (retained as fail-closed
compatibility symbols) and adds explicit AF_UNIX access through a separately
verified, private owner-supplied directory. On QNX 6.5 the service requests
getpeereid(); Linux host CI uses SO_PEERCRED. Kernel effective UID/GID **do
not identify a unique OEM process**; original SI owner/session verification
remains mandatory. No path or OEM process hooks are invented. See
`docs/R76_SI_UNIX_PEER_QNX65_CN.md`. SOURCE ONLY / NO INSTALL.


### R77 — retain SI socket reservation until OEM rollback confirms (NO INSTALL)

R77 fixes a native receiver lifecycle race: a failed/pending OEM STOP must not
unlink the SI AF_UNIX control pathname before the original AirPlay singleton
has actually restored. The old pathname remains reserved through retries,
preventing a second SI instance from stealing that endpoint. Reopening the
same in-use service object in a different private directory is also refused.
See `docs/R77_SI_SOCKET_RESTORE_BARRIER_CN.md`. Only host fixtures and CI
have been run; **this is not a car installation or wireless CarPlay proof**.

### R78 — nonblocking SI CPRX START client (NO INSTALL)

Adds a genuine AF_UNIX nonblocking CPRX v3 client in the existing native
service and an **explicit** original SI owner opt-in to the CarPlay pipeline.
The owner's periodic tick sends real 32-byte START, receives actual CPRA
acknowledgement, and never marks inner MFi or media ready from that ACK alone.
Legacy hand-crafted host START requests are no longer needed for the normal
test. UID/session/socket inode and restoration barriers remain enforced.
This is source-only: native `smartphone_integrator` runtime binding and real
phone video/audio are unverified. Details in
`docs/R78_SI_AUTO_START_CLIENT_CN.md`.

### R79 — matched SI START/STOP with OEM restore acknowledgement (NO INSTALL)

R79 adds a **nonblocking CPRX v3 STOP sender** beside the existing START
client, and connects it to `cp_cloud_graceful_native_end()` on the original
SI owner thread. Streaming native media remains protected: STOP receives an
explicit BUSY and must be retried after media is independently observed idle.
A successful STOP needs the OEM's original `stopServer`/interface-restore
barrier and the actual AF_UNIX acknowledgement; writing STOP bytes is not
success. Wired CarPlay preemption remains a separate immediate-priority
recovery path. An old START acknowledgement also ceases to be valid if its SI
session has ended. Host-only fixtures do not establish vehicle compatibility.
See `docs/R79_NATIVE_GRACEFUL_STOP_CN.md`.

### R80 — original SI owner-loss recovery, wired priority, interrupted operations (NO INSTALL)

The native owner loop now detects wired takeover *before* processing any
wireless START/STOP and retries pending OEM restoration on later ticks. A
lost original SI lease or fatal authenticated link now enters mandatory,
retryable native cleanup instead of leaving DIO alive behind a stopped flag.
An abandoned in-flight CPRX START/STOP keeps its private AF_UNIX lease until
the OEM restore barrier succeeds. Added actual local-socket negative tests.
These are source/host/OEM-stub checks, not a functional iPhone or QNX car test.
See `docs/R80_NATIVE_SESSION_WIRED_ROLLBACK_CN.md`.

### R81 — distinct SI/DIO process boundary (NO INSTALL)

Original MU1005 static ELF metadata separates `smartphone_integrator`
(`libirc_mmx_adapter.so`, no direct `libairplay.so` DT_NEEDED) from
`dio_manager` (`libairplay.so`, `libiap2client.so.1`). R81 adds a
**process-separated** SI CPRX v3 client that does not hold or dereference a
DIO `MuCprxService *`. A real Linux fork-based START/STOP roundtrip verifies
socket protocol boundaries. QNX production SI/DIO identity callbacks and
actual CarPlay media events are still missing. See
`docs/R81_SI_DIO_SEPARATE_PROCESS_IPC_CN.md`. No OEM binaries are packaged;
**SOURCE ONLY / NO INSTALL**.

### R82 — SI-to-DIO read-only media observation across processes (NO INSTALL)

The separate SI process can now issue an authenticated CPRX v3 op 6 status
query and receive its **full 12-byte reply** from the original DIO-side
receiver, instead of mistaking an 8-byte START ACK for streaming media.
Status values 0/4/5/7 are one-time DIO observations; they are never accepted
as an iPhone MFi/video/audio/control proof. Linux fork() regression checks
pre-START BUSY, observed state 4, and unchanged native START/STOP counts.
See `docs/R82_SI_DIO_READONLY_MEDIA_STATUS_CN.md`. NO INSTALL.

### R83 — same-session SI/DIO media evidence snapshot (NO INSTALL)

R83 adds read-only CPRX v3 operation 11 over the real two-process AF_UNIX
channel. Its 28-byte reply returns native DIO media status, same-original-SI
inner iAP2/MFi observation, individually attested video/audio/MMI/steering
channel bits, and exact generation/epoch/session echoes. Missing native
witness callbacks fail closed; inconsistent responses and expired SI owner
clear all cached observations. Linux fork() and hostile-response tests pass;
OEM source callbacks are still HOST FIXTURES ONLY, not real vehicle bindings.
See `docs/R83_SI_DIO_SESSION_PROOF_CN.md`. SOURCE ONLY / NO INSTALL.

### R84 — SI remote-control pipeline integrated with original iAP2 state machine (NO INSTALL)

The former same-process prototype could not directly serve the original
MU1005 separate `smartphone_integrator` / `dio_manager` executables. R84
adds an SI-side **remote-only** path to `cp_cloud_pipeline`: it drives the
real AF_UNIX DIO START/ACK, existing iAP2 `0x4301`/peer ACK, R83 same-session
op11 proof, and a confirmed idle STOP/restore, without storing DIO pointers
in SI. A previous media snapshot expires in 500 ms and never represents
ongoing device authorization. Real DIO/MFi owner event sources remain
unbound, and **the verified remote wired-preemption/force-restore ABI is
still missing**, so no vehicle installation is safe. Full details in
`docs/R84_SI_REMOTE_PIPELINE_CN.md`.
