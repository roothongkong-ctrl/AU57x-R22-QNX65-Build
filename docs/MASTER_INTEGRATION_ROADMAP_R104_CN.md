# R104 — AU57x MU1005 无线 CarPlay 统一整合主方案

日期：2026-11-26
目标：2017 Audi A6L C7 / MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
发布状态：RESEARCH_ONLY / NO_INSTALL / safe_to_flash=false / vehicle_verified=false

## 0. 本文目的

本文不是重新设计一套无线 CarPlay，也不是新建第二套蓝牙、iAP2、AirPlay 或 SI/DIO 管线。它把 R41–R103 已有源码、原厂静态分析、状态机和上游代码复用归并成一条唯一集成路线，明确每个模块的职责、输入输出、依赖顺序和真实缺口。后续研发先检查本文及各模块测试记录，再决定是否修改，禁止重复造轮子。

## 1. 唯一采用的系统架构

无线 CarPlay 分为两段 iAP2，不能混为一个连接：

A. 蓝牙引导段（首次连接 / bootstrap）
iPhone Bluetooth ACL + SDP
→ MU1005 原厂 btstack / BlueSDK 的真实 iAP2 RFCOMM owner
→ R45 原厂 BlueSDK RFCOMM/SDP 服务端适配
→ R98 原厂 iAP2 owner 到本机 TCP 127.0.0.1:57342 的生命周期桥
→ R44 原厂 iAP2 + MFi CONTROL 状态机（AA00–AA05）
→ 无线 bootstrap 完成后，读取本车 Wi-Fi AP 的真实 SSID、安全类型、密码、频道
→ 在原厂 AirPlay receiver 已就绪后发送 CarPlayStartSession。

B. Wi-Fi 会话段（媒体与控制）
iPhone 加入 MMI Wi-Fi AP（5GHz 优先；硬件/配置不支持时回退 2.4GHz）
→ Bonjour/mDNS 发现
→ 原厂 dio_manager / libairplay.so 的 AirPlay receiver
→ 通过原厂 AirPlay event/control channel 建立会话内第二段 iAP2（inner iAP2）
→ 原厂 iAP2 client / DIO 解码、音频和控制
→ smartphone_integrator（SI）与 dio_manager（DIO）通过既有 CPRX v3 AF_UNIX 控制和状态通道协作
→ 原车中控 CarPlay 显示；保留现有仪表地图显示链路。

无线蓝牙引导 iAP2 与 Wi-Fi AirPlay 内层 iAP2 是两个阶段、两种传输上下文。蓝牙 ACL/RFCOMM 已连接不能当作 MFi 成功；AirPlay receiver 已启动不能当作视频/音频已流动；SI/DIO START ACK 不能当作媒体已活动。

## 2. 已有模块归并表（保留，不重写）

| 既有成果 | 唯一职责 | 本方案中的位置 | 状态边界 |
|---|---|---|---|
| R41 / R44 | 原厂 MFi CONTROL、AA00–AA05、iAP2 CSM 顺序 | 蓝牙引导段认证 | 状态机已实现；真实 OEM owner / iPhone MFi 结果未证明 |
| R45 / R88 | MU1005 BlueSDK RFCOMM 注册、SDP、事件 RX/TX、连接归属 | 蓝牙 RFCOMM 服务端 | 原厂 btstack SHA/40 指令静态核对；真实 iAP2 owner 线程、SDK 运行时 ABI 未绑定 |
| R87 | 配对后延迟发起原厂 iAP2 client open、SDP 检查、有限重试/取消/USB优先及 MFi handoff | bootstrap 连接管理 | 生产 callbacks stock_open_iap2 / stock_cancel_iap2 尚缺真实 MU1005 实现 |
| R89 / R90 | RFCOMM 帧桥到 R44；区分 RFCOMM CONNECT 与完整 iAP2 SYN/LSP/ACK/MFi | bootstrap 数据面 | 主机 socket 与合成认证测试，不是 iPhone/实车认证 |
| R92 / R93 / R97 | btstack preload/线程/HFP GOT 只读观察 | 仅用于进程/线程调查 | HFP 观察不是 iAP2 owner 证明；不允许用 HFP 线程替代 iAP2 |
| R98 | 原厂 iAP2 owner → 127.0.0.1:57342 → R45 native attach/tick/withdraw | 唯一原生调用交接层 | C/QNX 源码路径与主机 socket 测试已建；真实 owner provider 未实现/未证明 |
| R99 / R100 | 原厂配置双备份/双配置生成、工厂/软件 graft 路由门禁、原厂 ELF 盘点 | 配置和安全决策层 | 离线生成与门禁通过；实际 connectivity 配置及运行时 owner 证据仍缺 |
| R101 | RFCOMM SCN 1–30 范围、注册参数快照、TOCTOU 修复 | R45/R98 注册参数前门 | HFP=7、SIM=4 的静态调用点已证；iAP2 专用 SCN 与完整私有 API ABI 未知，禁止猜 0/4/7 |
| R102 | 原样复用 LuKa EIR 广播组件 | 广播层 | 上游 EIR 源码可复用；仅在真实 iAP2 RFCOMM/SDP 成功后才可发布无线 UUID |
| R103 | LuKa 原版 iAP2 LinkEngine + R45/R88 RFCOMM 帧桥 | iAP2 link layer | 原始上游文件哈希一致，Rust 主机测试/本机 TCP lab；不包含 MFi、BlueSDK 注册、Wi-Fi、AirPlay；QNX Rust 运行时未证明 |
| R50 / R71–R80 | AirPlay/DIO 单实例、SI owner、START/STOP、媒体观察、owner loss/wired restore | Wi-Fi 会话和生命周期 | 主机/fixture 测试；真实 OEM SI/DIO 回调与事件源未绑定 |
| R81–R85 | SI/DIO 分进程、AF_UNIX CPRX v3、状态快照、同 session 证据、有线抢占与恢复 | SI/DIO 跨进程控制 | 协议和 host fork/socket 测试；真实 QNX peer/process identity、OEM restore 回调仍缺 |
| R86 | MU1005 原厂 AirPlay server/session delegate ABI 和 DIO GOT 关系 | AirPlay callback adapter | 静态 ABI 结构已还原；回调尚未注入原厂进程，媒体未实测 |
| R96 | 离线事务备份/替换/健康检查/恢复及 review SD 包 | 最终交付基础 | 仅主机 review 事务，不是生产车机安装器 |
| R100–R103 | 工厂/graft 路由、参数审计、上游 EIR 和 iAP2 link 复用 | 最新一致的整合开发基线 | 源码研发分支；禁止刷车 |

R111 的 Congo/MHI2Q 研究只作为架构参照：它支持“原厂 btstack + Bluetooth bootstrap + 原厂 AirPlay receiver + session-side iAP2 + USB 优先”的方向。它与 R87 已有的 Congo 适配研究重复，不再另起一条 R111 实现线。MHI2Q 的二进制、绝对地址、build fingerprint、driver/HMI 文件不得移植到 AU57x。

## 3. 唯一主线与分支管理

- 以 mu1005-wireless-implementation-r103-20261125 作为目前可读、模块说明完整的集成基线；先做完整源文件清点和 CI 复跑。
- R41/R44、R45、R87–R103 作为模块来源和测试证据，不要把每个历史分支当成独立产品拼装。
- mu1005-r142-qnx-native-verify / mu1005-r143-combined-owner-observer 等后续观察实验分支只在核对源码差异、依赖和测试后择优摘取；其 README 所示 R137 仅证明隔离 preload constructor / 同 PID 只读自映射观察，不证明 OEM 蓝牙注册、iAP2 owner 或运行时 hook。不得仅因版本号更大就覆盖主线。
- x4.3-integration-plan-20261126 属于早期实验/门禁线，不作为新的主线根目录，也不重复实现 R45/R98/R86。
- 每次整合必须写明源分支、提交 SHA、文件清单、许可证、ABI 差异、测试结果和回滚方式；同一职责只能保留一个生产候选实现。

## 4. 真正剩余的关键工作（按依赖顺序）

### Gate 1 — 真实 MU1005 iAP2 client owner / BlueSDK 调用时序（最高优先级）

目标：补齐 R87/R98 缺失的真实原厂 owner provider 和 client API，而不是继续写新的 RFCOMM 模拟器。

必须从已提供的私有原厂 ELF / 配置 / 启动文件中定位并证实：
- btstack 的 iAP2 串行事件线程/任务、服务 owner 和初始化/退出生命周期；
- RF_OpenClientChannel（或实际 SDK client open）以及 SDP 查询、连接、数据、断开/取消的精确调用 ABI；
- iAP2 专用 RFCOMM SCN / SDP profile 注册参数；R101 已证明 HFP=7、SIM=4，但不能推导 iAP2 值；
- SDK 锁、线程约束、回调重入规则、缓冲区持有/释放、撤销 barrier；
- connectivity_launcher / 原厂启动配置实际如何启动和恢复 btstack；R94/R100 的静态入口结论需与真实配置文件逐项对照。

交付物：一个可追溯的 MU1005_IAP2_OWNER_ABI.json（符号/偏移/调用点/寄存器参数/调用线程证据/置信等级）、只读探针和真实 callback provider 接口。未有证据时 provider 必须返回 unavailable，绝不拿 HFP 回调或参数 0/4/7 代替。

### Gate 2 — 将上游 iAP2 LinkEngine 变成目标可用模块

保留 R103 原版 LinkEngine 作为协议 link-layer 参考，不再重新写 Marker/SYN/LSP/ACK。先确定 Rust 2024/QNX 6.5 ARM32 标准库、线程/网络 ABI、静态/动态链接是否可用；若不可行，将上游 GPL 源码按许可要求移植为 C/C++，并用同一组上游向量测试确保行为一致。明确它只负责 iAP2 链路层，不提供 Apple MFi 身份或认证。

交付物：QNX ARM32 可链接模块 + 原版协议测试向量 + 帧桥双向/分片/断线/重连测试。未通过之前，R103 只留主机验证，不接原厂进程。

### Gate 3 — 真正 MFi / bootstrap / Wi-Fi handoff

把 R44 原厂 MFi CONTROL 与 R87 bootstrap 连接器绑定到 Gate 1 的真实 owner 回调；蓝牙链路完成后按协议完成 Identify/MFi，再从实际 AP 配置读取 SSID、security、password、channel。receiver ready 之前不发送 CarPlayStartSession。Wi-Fi 默认 5GHz、硬件/配置不支持时回退 2.4GHz；频段必须由实际无线驱动/AP 能力决定，不能假设支持。

交付物：同一 generation/session 的 BT→iAP2→MFi→AP handoff 事件日志；失败最多自动重试 3 次，带退避、超时、取消和手动连接提示；不能重复发送同一 session 的凭据或 start 消息。

### Gate 4 — 原厂 AirPlay / DIO / 内层 iAP2

复用 R86 的 server/session delegate ABI 与 R50/R71–R85 的 SI/DIO 跨进程协议。实现真正的 OEM callback provider，观测原厂 AirPlay receiver ready、Bonjour、session create/destroy、inner iAP2、音频/视频/MMI/方向盘事件。AirPlay server started、SI START ACK、DIO status snapshot 都不是媒体 active；必须有同一 session 的原厂 media/control witnesses。原车 dio_manager 与 smartphone_integrator 进程边界不可被跨进程指针打破。

交付物：真实 session 日志、音视频与控制证据、错误/断线清理和原厂 STOP/restore ACK。

### Gate 5 — 有线优先、自动重连、原车显示和安装恢复

沿用 R80/R85 的 wired takeover 与两阶段 restore；无线会话启动前检查 USB 有线 CarPlay owner，USB 插入时无线立刻让权；有线结束后按策略恢复蓝牙可连接状态。复用用户已配置的 MIB Cluster Map 有线仪表显示逻辑，验证无线 CarPlay 的中控画面及仪表显示是否通过同一原厂 DIO/地图源可用，不重做仪表项目。加入连接失败最多 3 次自动重试，之后提示用户手动连接。最后才把 R96 事务基础升级为真实 OEM 启动器/备份/健康检查/失败回滚，形成中文 SD 安装候选。

交付物：有线/无线反复切换、断电/重启、蓝牙/Wi-Fi 断连、恢复原厂、仪表显示回归报告；安装候选仍须单独安全审核。

## 5. 一次性静态审计清单（避免重复向用户索取）

优先使用已提供的固件包、app.img、mifs-stage2、btstack、bluetooth、ipod-drvr-iap2.so、smartphone_integrator、dio_manager、libirc_mmx_adapter.so、libairplay.so、libiap2client.so.1、iap2.cfg、dio_manager 配置和启动脚本。按文件 SHA256 建立索引。只在清单明确显示缺少某个具体文件时再向用户索取；不要求重新上传已有文件。

一次生成：
- OEM ELF / DT_NEEDED / imports / exports / relocation / strings / build ID / SHA256；
- 启动脚本与 connectivity.json / iap2.cfg 配置调用顺序；
- BlueSDK server/client API call graph；
- AirPlay server/session delegate ABI；
- SI/DIO 进程、IPC 和 owner map；
- 每个结论的来源文件、地址、反汇编片段、置信等级和是否需实车确认。

## 6. 自动化验证与验收定义

每个门槛分三层：
1. Host：协议向量、状态机、分片/超时/重试/断线/USB 抢占、socket IPC、恢复负例。
2. QNX ARM32：真实目标编译链接、ELF class/machine/ABI/DT_NEEDED/undefined symbols、QEMU 仅验证可加载/安全路径；不得在 QEMU 调用硬编码 OEM 地址。
3. Vehicle：真实 btstack owner、真实 iPhone MFi、Wi-Fi association、AirPlay session、音视频/控制、有线抢占、断电恢复。离线/模拟通过绝不标记 Vehicle PASS。

统一状态标签：DONE_HOST、DONE_QNX、DONE_STATIC_OEM、BLOCKED_OEM_OWNER、NOT_TESTED_VEHICLE、NO_INSTALL。每轮只报告新增的证据与差异，不重复抄历史测试表。

## 7. 最短可执行下一步

不要再扩展外围测试，也不要新写第二套协议。下一轮只做一件关键工作：

**把已有 MU1005 原厂文件做一次只读、哈希固定的 iAP2 client-owner ABI 总审计**，覆盖 btstack、bluetooth、ipod-drvr-iap2.so、connectivity_launcher、iap2.cfg，将 R45 server API、R87 client-open 缺口、R98 owner handoff、R101 SCN 不确定性、R103 link engine 输入输出放在同一份调用图里。先判断原厂 client-open 是否可直接复用；若能，接入 R87/R98；若不能，再研究最小、可撤销的 BlueSDK graft。只在证据明确后改代码。

**最终成功标准**：用户 iPhone 不插线自动连接；原车中控 CarPlay 完整工作；原车音频/触控/旋钮/方向盘可用；用户现有仪表地图显示链路保留；有线 CarPlay 随时优先；断线自动重试与手动提示可靠；可完整备份、安装、验证、失败回滚。当前这些最终标准尚未在实车达到。


## R105.002 补充：双层 Transport 边界（2026-11-26）

R105.002 对照既有 USB 插件 ABI、R104 iAP2 驱动反汇编和 R106/001 实车快照后，明确必须区分：
1. mm-ipod 外部物理插件 ipod-transport-%s.so（本车实证启动 -t usbdevice，USB 插件 ABI 已有历史复原）；
2. ipod-drvr-iap2.so 内部 BluetoothTransportComponent / WiFiTransportComponent 与 Link Transport vtable（+0x18/+0x28/+0x2c 已有静态反汇编证据）。

二者不是同一个插件/接口。R106 证明 USB 有线路径成功和 2.4GHz AP 有客户端关联；不能证明无线 iAP2/MFi 成功，也不能据此否定驱动内部 BluetoothTransportComponent。R105.002 是当前最新的边界核对文件，后续先复用已有 app.img/固件与 USB 插件样本，分别审计 mm-ipod loader 和 BluetoothTransportComponent producer，不再重做 HFP/PPS/packet parser/USB ABI。

详见 [R105.002 双层 Transport 边界核对](R105_002_TRANSPORT_LAYER_BOUNDARY_AUDIT_CN.md)。已知 mm-ipod 静态样本的预期 SHA256 为 4b87cd2d41ddcee2f38800feafe8b4b340e4f23e1d86c7e97af303522aa15d09；对应只读审计脚本为 tools/verify_mu1005_ipod_loader.py。该样本仍需与原始 app.img/车机实际版本核对，不能只凭字符串或旧报告宣称生产运行已验证。


## R105.003 补充：原厂 iAP2 路线复核与优先级（2026-11-26）

复核 R88/R99/R100/R104/R106 和历史交接后，当前主攻点调整为：
1. `bluetooth.enableIap` 的真实配置路径/schema/代码消费者/服务注册条件；
2. `ipod-drvr-iap2.so` 的 `iap2_init` → INI parser → TransportComponentIdentifier 映射 → BluetoothTransportComponent factory/vtable；
3. 该 component 的真实 BlueSDK/ASI/IPC producer 与双向 send/receive 关系。

R88 已有 RFCOMM server-first host adapter 与原厂 btstack 静态样本指纹；不重复重写。R100 的 `route_eligible_for_auto_activation=false`、stock iAP endpoint/service 未观察到、真实 owner 未证明仍是硬门禁。R99 配置暂存工具只证明受控离线生成，不证明配置可运行；其 `mac` 字段语义、transport ID/name 作用域、同时声明多 transport 的行为必须先查明。

R105.003 同时纠正研发优先级：mm-ipod 的 `-t usbdevice` 是 R106 证实的有线物理插件路径，不应先假设它就是蓝牙 iAP2 必经接口。详见 [R105.003 原厂 iAP2 路线复核](R105_003_FACTORY_IAP2_ROUTE_REVIEW_CN.md)。状态保持 NO_INSTALL。


## R105.004 补充：一次性只读车机证据采集

已基于 R93 旧脚本新增 `tools/R105_004_oem_iap2_readonly_probe.sh`：只读采集 bluetooth/btstack/connectivity_launcher/dio_manager/mm-ipod 进程与模块映射，查找真实 connectivity/bluetooth/iap2 配置路径，并将配置副本写入 SD 私有目录供离线分析。脚本不改车机配置、不重启服务、不注入代码。源码已提交但尚未实车执行；配置副本可能含 MAC，禁止公开上传。详见 [R105.004 说明](R105_004_READONLY_PROBE_CN.md)。
