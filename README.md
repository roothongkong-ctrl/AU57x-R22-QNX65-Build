> **主交接入口（2026-11-26）**：后续研发先读 [`docs/MASTER_HANDOFF_AND_NO_REPEAT_RULES_CN.md`](docs/MASTER_HANDOFF_AND_NO_REPEAT_RULES_CN.md)。该文件统一 22 步主计划、已有成果、GitHub/实车测试边界、CI 去重规则和下一任务；禁止从旧聊天摘要猜测进度。

# R103: 上游原版 iAP2 链路引擎和 AU57x RFCOMM 帧桥（源码版禁止装车）

本轮严格复用 `luka-dev/mib2q-carplay-rgi` 的 `iap2-link` 三个上游原始文件（逐个 Git object SHA-1 验证），新增 `integration/r103_iap2_bridge`，把 R45/R88 8 字节 little-endian RFCOMM 连接/数据/断开事件交给实际 LuKa `LinkEngine`，并将它的 Marker、SYN/LSP/ACK 结果包装为原来的 RFCOMM DATA。无 MFi 密钥、无原厂蓝牙注册、无 WIFI/AirPlay，只作主机验证。

执行：`bash ci/host_r103_upstream_link.sh`（需要 Rust 2024 edition 工具链）；原有 C 严格编译仍用 `bash ci/host_r95_source_compile.sh`。不装车。

# R101：MU1005 原厂 RFCOMM 通道范围与注册参数审计（禁止装车）

**本轮新增**：确认原厂 HFP=7、SIM=4 都流向同一原厂注册函数，并写进原厂通道对象；此参数看起来更像 SCN，而非已证实的 profile ID。修复原 C 代码错误的 1～6 通道限制为标准 1～30，将验证的 SCN 值冻结为一次会话快照，防止参数前后变化。详见 `docs/R101_RFCOMM_SERVER_CHANNEL_CN.md`。

**状态**：只读原厂静态 ELF + 可编译 C 源码、离线测试；没有确定原厂 iAP2 真正注册参数/所有者，**不准装车**。

---

# AU57x Wireless CarPlay – R100 reversible factory-to-graft branch

Status: **SOURCE ONLY / NO INSTALL / NOT VEHICLE VERIFIED**.

**R100 deliverables**: Original MU1005 exact-ELF feature audit; confirmed divergences from other MHI2 firmwares; native C fail-closed factory/graft selection; new guard on QNX R98-to-R45 BlueSDK attach entry; 44-module source compile and regression tests. R99 dual config generator remains as factory branch and R97/R98 remain experimental graft branch; both still require live OEM evidence.

Chinese explanation: `docs/R100_FACTORY_TO_GRAFT_CN.md`. Private OEM files are NOT included.

Reproduce with original files on a private machine:
```sh
R100_ORIGINAL_DIR=/path/to/MU1005_original_ELFs bash ci/host_r100_route_audit.sh
python3 ci/host_r100_continuous.py
```

The result `r100_result/R100_ORIGINAL_MU1005_ROUTE_AUDIT.json` contains hashes and offsets only. It must not be treated as an installation endorsement. **Do not disable MFi, modify the original `/eso/bin/apps/btstack`, replace factory `bluetooth`, or install any review ZIP to the car.**

---

## R102：直接复用上游 GPL 源码，不再重复造轮子

现已从 `luka-dev/mib2q-carplay-rgi` 精确引入 `wireless/graft/eir.c`，保证 1691 字节原文件与上游 Git blob 完全相同，并编译到 AU57x 的独立审计入口 `au102_eir_advertisement`。只在原厂 btstack、iAP2 RFCOMM 和 SDP 都已确认的情况下允许广播，保留有线 CarPlay 优先。测试：`bash ci/host_r102_upstream_reuse.sh`。

详情：`docs/R102_OPEN_SOURCE_REUSE_CN.md`。**该源码只证明一个通用组件可移植，不证明无线 CarPlay 已完成；禁止装车。**


## R105.005 — unified MU1005 wireless route review

Reconciles R88–R103 with the newer R142/R143 owner observers, ASI Bluetooth iAP proxy, SI/DIO CPRX, AirPlay second-stage iAP2 relay and read-only AP discovery. Corrects the older R99 assumption: do not enable the commented `iap2.cfg [bluetooth]` status section. The ASI IapDeviceServices raw-byte bridge is closed based on the prior 778-ELF app.img scan (proxy stub only, no server implementation). The next static target is the native `dio_manager → libiap2client.so.1 → /dev/ipod0` bidirectional path and the OEM AirPlay session callback/event-control owner. User band policy remains 5GHz preferred with 2.4GHz fallback. See [R105.005 route reconciliation](integration/mu1005_si_native/R105_005_ROUTE_RECONCILIATION_CN.md). NO INSTALL.


### R105.006 — `/dev/ipod0` Resource Manager owner trace

Static evidence narrows the likely provider to `ipod-drvr-iap2.so` dynamically hosted by `mm-ipod`: driver exports `ipod_module` and imports `ipod_resmgr_mount` plus QNX `iofunc_*`/message APIs; OEM loader instructions show mm-ipod injecting module owner and invoking init. DIO is the client using `libiap2client.so.1` with `/dev/ipod0`. Exact mount-path callsite and live vehicle PID remain unverified. Keep `-t usbdevice`; NO INSTALL. [R105.006 report](integration/mu1005_si_native/R105_006_IPOD0_RESOURCE_OWNER_CN.md).


### R105.007 — iAP2 client path/message boundary

Clarified that DIO passes a caller-supplied device path to libiap2client; /dev/ipod0 in the factory config is the likely source, but the config parse-to-callsite dataflow is not yet closed. Also separated iap2_connect/client MsgSend IPC from iap2_raw_msg_send and the driver's physical transport callbacks: raw_msg_send is not proven to be an arbitrary RFCOMM injection API. [R105.007 report](integration/mu1005_si_native/R105_007_IAP2_CLIENT_DEVICE_PATH_AND_MESSAGE_BOUNDARY_CN.md). NO INSTALL.


### R105.008 — transport layer reconciliation and R103 host gate

The current route separates (1) mm-ipod physical Transport plugins, (2) internal iAP2 TransportComponents in ipod-drvr-iap2.so, and (3) the btstack RFCOMM producer/owner. The R103 upstream LinkEngine is retained for protocol handling; R98/R45 remain the candidate OEM integration layer. The factory usbdevice path is preserved. A dedicated current-branch HOST-ONLY regression workflow now checks the upstream source hash, Rust bridge/TCP tests, and C framing tests. [R105.008 report](integration/mu1005_si_native/R105_008_ROUTE_LAYER_RECONCILIATION_CN.md). NO INSTALL.


### R105.009–R105.010 — owner nonblocking fix and classic MHI2 donor comparison

R105.009 makes the legacy native bind entry fail-closed so the BlueSDK/iAP2 owner callback cannot accidentally perform a blocking TCP connect; only a worker-prepared nonblocking/CLOEXEC fd may be adopted on the proven owner thread. R105.010 compares classic MHI2 MU1440, MHI2Q MU1316 and MH2p donors. It elevates the stock Type-3 transport path for investigation before duplicating the iAP2 link layer; R103 remains the fallback. No donor binary is transplanted and no vehicle install is authorized. [R105.009](integration/mu1005_si_native/R105_009_OWNER_NONBLOCKING_BOUNDARY_CN.md) · [R105.010](integration/mu1005_si_native/R105_010_CLASSIC_MHI2_DONOR_COMPARISON_CN.md).


### 22-step execution — Step 01 ABI map

Step 01 has mapped the MU1005 mm-ipod outer Transport loader and the seven OEM USB donor callback slots (ABI v2 / 0x200), reusing the prior 22/22 OEM static audit. A new host C layout gate verifies the documented ARM32 48-byte callback prefix and 36-byte module object; callback prototypes, loader lifecycle and runtime selected plugin remain explicitly unresolved. [R105.011 Step 01 report](integration/mu1005_si_native/R105_011_STEP01_MM_IPOD_PLUGIN_ABI_CN.md). No install.


### 22-step execution — Step 02 driver internals

Step 02 has now inspected the exact MU1005 `ipod-drvr-iap2.so` ELF from the user's prior private handoff archive (SHA-256 pinned). It maps the outer transport send/receive dispatch, Bluetooth iAP2 status/update handlers, Wi-Fi configuration message 0x5703, and transport-identification helpers. Crucially, `bt_*` status handlers are not RFCOMM byte callbacks; Bluetooth/Wi-Fi component constructors, vtables and the live BlueSDK-to-driver byte path remain unproven. [R105.012 Step 02 report](integration/mu1005_si_native/R105_012_STEP02_IPOD_IAP2_DRIVER_INTERNAL_MAP_CN.md) · [Static audit JSON](docs/R105_STEP02_DRIVER_STATIC_AUDIT.json).


### 22-step execution — Step 03 plugin loader and USB ABI

Step 03 has statically mapped the original `mm-ipod` dynamic transport loader and cross-checked its callback context against both stock USB Device and USB Host plugins. The inspected handoff archive contains only those two `ipod-transport-*` modules; it also contains Bluetooth iAP service-management evidence, but no proven raw RFCOMM-to-iAP2 transport. This archive-scoped absence is not proof of absence from all firmware partitions. [R105.013 Step 03 report](integration/mu1005_si_native/R105_013_STEP03_MM_IPOD_PLUGIN_LOADER_AND_STOCK_USB_ABI_CN.md) · [Audit JSON](docs/R105_STEP03_MM_IPOD_PLUGIN_LOADER_AUDIT.json).


### 22-step execution — Step 04 BTStack iAP2 owner boundary

Step 04 reuses the earlier R88/R101/R45/R98 evidence and adds a static cross-layer map of the MU1005 `bluetooth` app, `btstack`, and ASI iAP proxy. It confirms iAP enable/service markers, generic SDP/RFCOMM infrastructure and DSI/RPC service classes, but does **not** identify the iAP2-specific RFCOMM owner, SCN, callback ABI or live owner binding. [R105.014 Step 04 report](integration/mu1005_si_native/R105_014_STEP04_BTSTACK_IAP2_OWNER_BOUNDARY_CN.md) · [Audit JSON](docs/R105_STEP04_BTSTACK_IAP2_OWNER_STATIC_AUDIT.json).
