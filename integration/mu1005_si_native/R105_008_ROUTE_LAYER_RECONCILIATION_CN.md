# R105.008｜iAP2 / Bluetooth / USB Transport 分层复核与下一段研发决策

日期：2026-11-26
目标：2017 Audi A6L C7 PA / MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
状态：STATIC_AND_HOST_REVIEW / NO_INSTALL / vehicle_verified=false

## 本轮做了哪些额外核对

交叉阅读并核对了 R98、R99、R100、R101、R103、R105.006、R105.007 的原始报告、源码、测试和 CI 工作流。发现必须明确区分三种“传输”：

1. **mm-ipod 外层物理 Transport 插件**：例如当前原厂启动选择的 usbdevice 插件。由 mm-ipod 的模块加载 ABI 管理，给 iAP2 driver 提供 send/receive 接口。
2. **ipod-drvr-iap2.so 内部 iAP2 TransportComponent**：二进制字符串/组件层出现 BluetoothTransportComponent、WiFiTransportComponent 等。这是 iAP2 协议驱动内部的组件，不等于 mm-ipod 外层存在一个可直接选择的 bluetooth 物理插件。
3. **蓝牙 RFCOMM 数据入口**：MU1005 btstack / BlueSDK 真实注册服务、owner callback、channel 和数据流。这一层目前尚未证明已连到第 1 或第 2 层。

R105.006 已说明 driver 的 Resource Manager 与 mm-ipod 模块宿主关系是高置信静态架构推导；R105.007 的设备路径到 connect wrapper 参数仍有待配置读取点闭环。不能将这两个判断扩大成“无线 RFCOMM 已经进入 driver”。

## 证据与路线对照

| 路线/模块 | 已经有的证据或实现 | 尚缺关键项 | 当前用途 |
|---|---|---|---|
| 原厂 stock iAP2（R99） | 原厂 bluetooth ELF 有 enableIap / iapEnabled / SERVICETYPE_IAP2 标记；driver 有 BT/Wi-Fi 内部组件标记；离线双配置生成器 23/23 | 真实配置文件、真实启用效果、driver 物理 BT producer、MFi、AirPlay | 保留为优先只读调查路线，不盲改开关 |
| R98 原厂 BlueSDK handoff | TCP 本机交接源码、native attach/withdraw 生命周期、host 测试 | 真实 iAP2 owner 线程、服务注册 profile 参数、原厂调度接入 | 可作为 AU57x 原生 RFCOMM 接入框架 |
| R103 LuKa iAP2 LinkEngine | 锁定上游源码、C 帧桥、Rust ReviewBridge、真实 localhost TCP lab | MU1005 原厂 RFCOMM owner 与 frame producer/consumer、QNX Rust 目标运行、MFi/AirPlay | 可复用的协议层，不可直接安装 |
| mm-ipod 外层 USB Transport | R44 静态 ABI 机器码/符号核对；当前启动使用 usbdevice | 不影响现有有线的第二物理 Transport 注入证据 | 必须保留，不能替换 |

## 本轮决策

采用“双层研发、单一接入点”的策略：

- **协议层复用 R103**：不重新实现 LuKa 已有 iAP2 link negotiation，不把 R103 当作 Apple MFi 或 CarPlay 已成功。
- **原厂接入层沿用 R98/R45**：继续寻找 MU1005 真正 iAP2 owner callback 和 BlueSDK 注册 ABI；不要使用 HFP/SIM 线程代替 iAP2 owner。
- **stock iAP2 作为只读对照**：R99 的开关工具只允许离线审查与暂存，现阶段不改真实车机配置。
- **物理 Transport 层不动**：保留 mm-ipod -t usbdevice、有线 CarPlay 和原厂启动参数。

## 已检查的 CI 结果

2026-11-26 当前分支最近一次完整工作流：
- R103/原生 CPRX 工作流 commit：4f360be52325b8e4f9cba5ff100f46b7c41b9480；run 45754659395，success。
- MU1005 unified development 工作流 commit：ebb613c4ede657f2d21f69c7b05e9d0f9ffa90c1；run 45754654178，success。
- QNX ARM 编译链接步骤、原厂 BlueSDK handoff revoke/recovery 测试、各并行 host 测试组、artifact 汇总均为 success。
- 这些 CI 不包含真实车载 iPhone 无线配对或 MFi 硬件认证；R103 独立 Rust LinkEngine 工作流此前只在专用分支触发，当前分支还需要单独覆盖。

## 下一段研发任务（按优先级）

1. 为当前分支新增独立 R103 host gate：验证上游文件 hash、原版 LinkEngine tests、AU57x 帧桥 tests、真实 localhost TCP lab；明确标记 HOST_ONLY。
2. 反汇编原厂 libiap2client.so.1 的 connect/raw_msg_send/disconnect，恢复 QNX MsgSend 请求 envelope、句柄及返回值。
3. 对 ipod-drvr-iap2.so 的 ipod_resmgr_mount、Resource Manager dispatch 和 driver send/receive 回调做静态调用图；区分 client IPC 与物理 Transport。
4. 对 mm-ipod 外层 plugin ABI 做完整对照，禁止未经证据新增或替换 bluetooth plugin。
5. 对 MU1005 btstack 的 iAP2 owner/thread/注册调用做真实交叉引用，确认唯一 owner、服务注册参数、数据 callback 生命周期，再接入 R103。
6. 最后才做 AirPlay/MFi/Wi-Fi session 验证和 SD 安装回滚包。

## 验收边界

- HOST Rust/C tests pass ≠ QNX target pass。
- QNX ARM ELF compile/link pass ≠ OEM runtime pass。
- OEM static import/string/ABI pass ≠ RFCOMM 已连接。
- RFCOMM/iAP2 link pass ≠ Apple MFi / CarPlay session pass。
- AirPlay server symbols pass ≠ iPhone screen/audio 已工作。

**结论：现在最有效的推进点不是修改 iap2.cfg 或 USB 插件，而是同时补齐 R103 的当前分支 host 回归和 MU1005 原厂 iAP2 client/server IPC ABI。**

installable=false / safe_to_flash=false / vehicle_verified=false / mfi_apple_verified=false。

## R105 HOST gate 实际日志（补充）

新建的当前分支专用工作流已在 commit `55858991ae6192c1c98af0f752a15a0dd4db8c6c` 运行成功：
- Rust AU57x bridge tests：7/7 PASS。
- localhost TCP lab tests：2/2 PASS（实际 TCP loopback 双向收发）。
- LuKa 上游 LinkEngine tests：13/13 PASS。
- C 原 RFCOMM frame framing test：PASS。
- 原版上游三个文件 hash 校验：PASS。
- 工作流：`45755107085`，全部步骤成功。

本轮最新分支 commit `8e72e5467acf29165f342edd3d10dc981e74e19d`：
- SI real CPRX host 工作流 `45755163884`：SUCCESS。
- unified development 工作流 `45755163707`：截至本报告更新时仍在 QNX 工具链/原生库构建阶段；其余并行 host 组均已 SUCCESS。最终结论以该 run 完成结果为准。

以上仅说明协议引擎和帧桥在主机上的回归已实际跑通，不证明 MU1005 蓝牙 owner、QNX runtime、MFi 或 CarPlay 实车连接。
