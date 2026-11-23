# R97 — 原厂 HFP 回调 GOT 观察器 + btstack 线程归属接线（仅源码）

**R97 新增工作：** 从你原厂 MU1005 `btstack` 的真实 ELF 动态重定位表，定位了 HFP `HfRfCallback` 的 `R_ARM_GLOB_DAT` 指针槽 (`0x2c23c8`，原目标 `0x2481f0`)，以及 SIM 的只读参考槽。新增可选的原厂 HFP 回调透传观察器和真实 pthread 线程入口追踪；主机 9 组连续回归全部通过，42 个 C 模块严格编译通过。

**边界：** 这不是 iAP2 CarPlay 注册，原车实际回调和 iPhone MFi / AirPlay 仍未验证；没有原厂 `connectivity.json` 或 OEM 的运行时 owner 证据，R97 **不是可装车包**。完整技术说明在 `docs/R97_STOCK_HFP_GOT_THREAD_OWNER_CN.md`。

---

# R96: steps 18-19 (host lab + non-executable release review; NOT A CAR INSTALLER)

R96 adds a generated, SHA256-pinned supervised host lab for actual C file backup, replacement, candidate-process health and rollback, and a deterministic verified non-executable SD review bundle. New tests and Chinese handoff: `docs/R96_STEPS_18_19_CN.md`.

Commands: `bash ci/host_r96_oneclick.sh`, `bash ci/host_r96_candidate.sh`, `bash ci/host_r96_to_step19.sh`. Production OEM process stop/start, live iAP2/MFi, 2.4 GHz AirPlay, boot recovery and vehicle wiring are NOT proven. Do not install any R96 archive on a vehicle.

---

# R95：直接连续推进第 3～17 步（离线工程；禁止装车）

本版将第 3～17 步合成**一次不中断的离线回归**，`tools/r95_stage_review.py` 实际运行 **15/15 组主机测试通过**，每个步骤独立记录真车欠缺项。`bash ci/host_r95_to_step17.sh` 可重新生成 `r95_result/R95_STEPS_03_TO_17_EVIDENCE.json`。**这里的 15/15 是离线测试通过，不是 15 个真实装车步骤全部完成。**

实质新增：`tools/r95_sd_plan_codegen.py` 将旧/新文件 SHA256、权限、所有者固定生成真实 C `txn_spec` 源码，和现有 `tools/review_txn/txn.c` 共同编译运行，执行 `trial`/`recover`、备份检查、失败前拒绝并测试 11 种正反场景；生成器**仅**支持明确标记 `au57x-r95-*` 的主机试验目录，不能装车。还修复 GCC 下载 `curl exit 18` 的一次性网络问题，保留工具链源码 SHA256 验证，已在 CI 分支增加有限重试。

**绝对不要求现在上车。** 既不导入私有原厂二进制，也不宣称 `btstack`、iPhone、Wi-Fi 或 MFi 已在真实车机接通。原厂准确 `connectivity.json` 与回调 owner 等生产证据仍需最后一次车机验证，参见 `docs/R95_TO_STEP17_MULTI_TASK_CN.md`。

- **主机验证**：`bash ci/host_r95_to_step17.sh && bash ci/host_r95_source_compile.sh`
- **研究范围**：步骤 3～17 离线集成；步骤 18～20 尚待装车候选/最终验收
- **发布类型**：源码 + 测试，不是可装车 SD 更新包，`installable=false`。

---

# R93：原厂 btstack 动态调用核验 + SD 卡一键取证（研究与诊断版）

**已完成：** 用户原始 MU1005 `btstack` 的 QNX 动态调用（4/4 ARM BL → PLT、`R_ARM_JUMP_SLOT pthread_create`）静态验收；预加载器加入真实进程名检查；实际动态预加载的主机测试通过。

**尚未完成：** 必须在车上看到原厂 `btstack` 的 `LD_PRELOAD` 实际加载记录、对应 PID 和内存映射，才能把“第 2 步第 1 项”标记为完成。现有 R93 SD 脚本只收集事实，不安装也不注入，因此不会假称已加载。

**SD 操作：** 将 `R93_SD_MIB_ROOT/mod/custom.sh` 放到已有 M.I.B. SD1 卡根目录的 `/mod/custom.sh`（如果原来存在，先备份），绿屏 M.I.B. → Advanced Settings → Run Custom Script；结果在 SD 根目录 `R93_BTSTACK_PROOF`。不需要 SSH。完整步骤看 `docs/R93_FIRST_ITEM_CN.md`。

**本地运行：** `bash ci/host_bt_preload_probe.sh && bash ci/host_r93_plt_audit.sh`；私有原厂样本：`R93_PRIVATE_BTSTACK=/path/to/btstack bash ci/host_r93_plt_audit.sh`。

**不含原厂二进制。禁止将此源代码 ZIP 当作可刷机固件。**

---

# R92：原厂 btstack QNX LD_PRELOAD 入口与线程追踪（SOURCE ONLY）

**本版新增真实进程入口：** `bt_preload_probe.c` 实现已核验 MU1005 ELF 哈希的 QNX 动态预加载构造器、`pthread_create` 代理和日志观察。离线从原厂 ARM ELF 找出 4 个调用点，并可用主机真实 `LD_PRELOAD` 实测执行。新增 QNX ARMv7 `.so` 编译任务。**这不是实际车上蓝牙 RFCOMM 回调，也不能声称 iPhone 已接通。**

参见 `docs/R92_STOCK_BTSTACK_PRELOAD_THREAD_ENTRY_CN.md`。执行 `bash ci/host_bt_preload_probe.sh` 检查注入机制。安装仍阻塞：需真实原厂进程运行及 BlueSDK owner 回调识别。**NO_INSTALL / safe_to_flash=false。**

---

## R91 FAST-CONNECT EXPERIMENT (2026-11-22)

User-priority change: attempt stock MU1005 Bluetooth iAP2 first connection as early as possible. See `docs/R91_FAST_CONNECT_CN.md`. R91 adds an **opt-in, unverified RFCOMM argument-0 trial** (`AU57X_FAST_CONNECT_EXPERIMENT=1`) only if the original owner does not provide a verified argument. Original MFi and iAP2 protocol gating remain intact. This is a compiled research source; **it does not contain a usable btstack loader, is not a working wireless CarPlay implementation, and is NOT INSTALLABLE**.

## R90 / Original MU1005 Bluetooth iAP2 handshake readiness (2026-11-22) — SOURCE ONLY

R90 fixes a genuine R89 integration bug: **RFCOMM CONNECT was being reported as iAP2 `LINK_READY` before an iAP2 handshake even began**. The R45 socket-to-R44 `cp_wcp_r45_remote` path now requires a live authenticated original transport **and** the completed R44 peer iAP2 `SYN/LSP/ACK` (`CP_LK_NORMAL` + `have_peer_syn`). R44 original MFi `AA00..AA05` is still a distinct *mandatory* authentication step before wireless CarPlay begins.

`ci/host_wcp_r45_remote.sh` now uses real split TCP frames to verify all link phases and USB/owner refusal. `ci/host_wcp_r45_remote_mfi.sh` additionally runs the full R44 AA00/AA02/AA05 state machine with clearly synthetic OEM cert/signature fixtures and a real localhost TCP stream, including irreversible revocation. These are **not** Apple or in-car successes.

The original MU1005 `btstack` native owner hook, proprietary iAP2 RFCOMM registration mode and actual iPhone/MFi outcome remain **unverified**. The unchanged native callback adapter requires independently proven `iap2_profile_proven`; there is no SD installer or OEM code in this repository. See `docs/R90_MU1005_R45_IAP2_NORMAL_MFI_GATES_CN.md`. `installable=false`, `vehicle_verified=false`, `safe_to_flash=false`.

---

## R89 / MU1005 original btstack cross-process RF bridge (2026-11-22) — SOURCE ONLY

R89 fixes the R88 cross-process pointer assumption by connecting the actual **R45 RFCOMM normalized TCP frames (127.0.0.1:57342)** through the existing R44 socket/iAP2/MFi state machine to a new `cp_wcp_r45_remote.[ch]` adapter. The transport proof checks an accepted local socket, original owner, independent paired MAC, server publication, same generation, and wired priority continuously. Existing R44 MFi proofs are still necessary; a test owner or socket frame never authenticates the phone.

Original user-owned MU1005 `btstack` ARM calls establish that the OEM RFCOMM register third argument differs across services (HFP=7, SIM=4). It is *not known* for iAP2. The former R45 literal zero is replaced by a mandatory original-owner `iap2_profile_proven()` callback that fails closed when absent, with no HFP/SIM guess. Private binaries are not redistributed. See `docs/R89_REAL_BTSTACK_TO_R44_R87_CN.md`, `docs/R89_MU1005_RFCOMM_REGISTER_CALLERS.json`.

Run `bash ci/host_wcp_r45_remote.sh` and `python3 -m unittest discover -s tests -p test_mu1005_rf_register_sites.py -v`. **Still missing actual btstack process/SDK event owner binding, proven iAP2 registration mode and real iPhone/MFi exchange. `vehicle_verified=false`, `installable=false`, `safe_to_flash=false`.**

---

## R88 / MU1005 original Bluetooth path (2026-11-22) — SOURCE ONLY

R88 now compares the **user-owned, SHA256-pinned actual MU1005** `btstack` and `bluetooth` against the public MU0678 `IapDevice`/`IapServices` trace (https://github.com/joeyQuery/MHI2-wirelessCarPlay). The original MU1005 Bluetooth control binary has `bluetooth.enableIap` and `SERVICETYPE_IAP2`, while the original MU1005 `btstack` lacks **literal** `IapDevice`, `IapServices`, `/dev/iapDevice`, `CIapBTChannel` identifiers. The native stock RFCOMM/SDP and existing OBEX client paths **are** found. This does not prove that MU1005 lacks a dynamically formed iAP2 endpoint or that the native client-open ABI is known. Pinned ARM entry-word check: 40/40. OEM binaries stay private. See `docs/R88_MU1005_BT_IAP_PATH_CN.md` and the reproducible JSON.

The **new functional C99 bridge** `cp_wcp_r45_ingress.[ch]` uses genuine, same-generation, original RFCOMM server connection ownership and R45 `AuRfIngress` to populate R87's `cp_wcp_bt_ops`. It only reports transport connected when the real R45 RF epoch/MAC, owner, pairing, published SDP and upstream IPC transport all still match; USB has priority. The *unverified* client-open/SDP methods are deliberately not implemented. **Step 2 is partial**, no executable native callback loader, no car validation, no SD installer and no claimed original MFi success.

Run `bash ci/host_wcp_r45_ingress.sh` and `python3 -m unittest discover -s tests -p test_mu1005_bt_iap_path_audit.py -v`. Original files must be supplied privately to `tools/mu1005_bt_iap_path_audit.py`, not committed. `installable=false`, `safe_to_flash=false`.

---

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

### R85 — wired-priority cross-process original-DIO restore (NO INSTALL)

Adds opt-in CPRX v3 op 12 for **independently attested physical wired takeover** on *both* SI and DIO original owners. Unlike normal media-aware STOP, op12 can ask the original DIO backend to restore while media is streaming, but its ACK cannot precede completed original STOP/interface restoration. On failure or missing witnesses, the remote pipeline stays in retryable pending state. A separate SI final teardown witness must complete before full success. Host-only regression uses actual AF_UNIX + OEM test callbacks; neither production callback has been located in original binaries. This code must not be installed. See `docs/R85_SI_DIO_WIRED_RESTORE_CN.md`.

### R86 — verified exact MU1005 DIO/AirPlay two-level callback ABI (NO INSTALL)

A read-only SHA-256-pinned ARM32 ELF auditor reconstructs the **single** stock
`AirPlayReceiverServerSetDelegate` registration call and the nested
`AirPlayReceiverSessionSetDelegate` call.  Original `libairplay.so` machine
code copies **60** and **56** bytes, respectively.  Original DIO ARM PIC/GOT
relocations prove 11 server callback slots and 10 session callback slots.
Neither an OEM hook nor real vehicle pairing/audio/video is implemented.
`NO_INSTALL` remains mandatory. See
`docs/R86_ORIGINAL_OEM_DELEGATE_ABI_CN.md`.

### R87 — MHI2Q WCP reference adapted to AU57x BT-first native pipeline (NO INSTALL)

R87 reimplements the publicly documented **car-initiated Bluetooth iAP2**
bootstrap as owner-attested C99 code, with delayed client open, SDP check,
bounded retries, pending cancellation barrier, generation checks and wired/USB
priority. A new handoff function joins that genuine BT evidence with existing
R44 OEM iAP2/MFi CONTROL and the R85 cross-process SI/DIO wireless pipeline;
no fake BT-only success can authorize CarPlay. The real AU57x SDK client-open
and cancellation callbacks are **not yet located or implemented**, and the
real car's AirPlay/media proof remains unbound. No executable vehicle test
package, donor blobs or SD installer. See `docs/R87_CONGO_WCP_AU57X_INTEGRATION_CN.md`.

## R94: 持续完成多项（2026-11-22）

这轮完成 1) R93 QNX 云端结果验收；2) 原厂 MU1005 `connectivity_launcher` 精确 SHA 的启动器入口审计；3) 只面向电脑离线预览的 `connectivity.json` 可回退启动配置生成器；4) 保留 R90/R91 蓝牙/iAP2/MFi 和 SD 事务基础测试。**R94 不能直接刷进车机**，尚无真实 BlueSDK 运行时回调绑定。详细记录 `docs/R94_MULTI_STEP_PROGRESS_CN.md`。
