# 无线 CarPlay 项目｜主交接总表与防重复研发规则

更新日期：2026-11-26
唯一开发分支：`mu1005-si-cprx-real-review`
本次整理基线 commit：`2f5adce342796b7e8beeb122804cf730a373f588`
适配目标：2017 Audi A6L C7 PA / MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
发布边界：SOURCE REVIEW ONLY / NO INSTALL / NOT VEHICLE VERIFIED

## 0. 本文件的用途

本文件是后续对话与研发的唯一交接入口。先读本文件，再按链接读取对应原始报告；不得从旧聊天摘要重新猜状态。除非出现新原厂文件哈希、新源码提交或新的实车日志，不重复已经完成的静态审计、依赖检查和 CI。

## 1. 先纠正进度数字：正式主计划是 22 步，不是 25 步

- 当前仓库 README 与 `integration/mu1005_si_native/README.md` 明确使用 **22-step execution**。
- 之前回复中的“25 步计划”没有在当前主分支找到对应的权威计划文件，属于错误计数，废止。
- R95/R96 的“步骤 3–20”是更早一套离线集成/安装候选工作分解；它与当前 R105 的 22-step 原厂 Transport/owner 路线不是同一套编号，禁止把两套编号混合相加或互相换算。
- R105.017–R105.020 是对 mm-ipod / libiap2client / Resource Manager 同一数据通道的连续深挖；R105.021–R105.026 是 AirPlay / Wi-Fi 数据面专项报告。这些 R105.x 小版本号不是新的主计划步骤。

### 22 步主计划当前状态（严格口径）

| 主步骤 | 当前状态 | 已有证据 | 尚缺的闭环 |
|---|---|---|---|
| 01 外层 Transport ABI | STATIC REVIEW DONE | loader、7 个 OEM USB donor callback 槽位、ARM32 layout host gate | callback 原型/生命周期、运行时插件实例 |
| 02 ipod-drvr-iap2 内部 | STATIC REVIEW DONE | 原厂 ELF、send/receive dispatch、BT 状态/Wi-Fi 配置消息映射 | BT/Wi-Fi component 构造、真实物理 transport vtable |
| 03 mm-ipod 插件加载 | STATIC REVIEW DONE | 动态 loader、USB Device/Host ABI 对照 | 无线 transport plugin 的实际加载和 producer |
| 04 BTStack iAP2 owner | STATIC REVIEW DONE（边界已知） | 通用 SDP/RFCOMM、iAP/ASI 控制面静态证据 | iAP2 专属 RFCOMM owner、SCN、回调 ABI、运行时绑定 |
| 05 ASI/SI 控制面 | STATIC REVIEW DONE | EMS/app.img 778 ELF 审计；ASI IapDeviceServices 无 raw-frame provider | 不再走 ASI 裸字节桥；真实控制状态仍需运行证据 |
| 06 Resource Manager / client-server transport | PARTIAL / ACTIVE | /dev/ipod* mount、libiap2client MsgSendv、mm-ipod io_msg 转发链已部分恢复 | context+0x24 callback writer、0x9999/0x3000 opcode dispatch、driver transport 接线、BlueSDK producer |
| 07–22 | NOT CLOSED | 旧 R95/R96 中有部分离线模型、host harness、恢复事务与媒体契约 | 必须逐项映射到 22 步正式验收表后再标状态；不得仅因旧 host tests 通过就记为生产步骤完成 |

**当前有效结论：前 5 步完成的是静态审计/主机结构门禁，不是无线功能完成；第 6 步仍在进行；第 7–22 步没有在本轮整理中虚报完成。** 因此本文件不使用“10/22”“6/25”这类把报告数量当成生产步骤的进度数字。

## 2. 现有成果地图：已做过什么，不要重做

### A. 上游协议与无线引导
- R103：复用 luka-dev 上游 iAP2 LinkEngine；host Rust/C bridge 测试通过。它是协议层，不是原厂 RFCOMM owner。
- R45/R88/R89/R90/R91/R92/R93/R97/R98/R101/R102：已积累 MU1005 蓝牙、RFCOMM、HFP/SIM、thread/preload、SDP、MFi gate、EIR、owner handoff 等静态与 host 研究。
- R87：参考 MHI2Q 的 Bluetooth-first bootstrap 已做成 owner-attested 模型；真实 MU1005 client-open/cancel callback 仍未绑定。
- R100：factory/graft 选择与 owner 边界；不得禁用 MFi、替换原厂 btstack 或绕开认证。
- 不能把 HFP=7、SIM=4 或其他 profile channel 直接当成 iAP2 SCN；真实 iAP2 SCN/owner 未证明。

### B. mm-ipod / iAP2 client-server 深挖
- R105.011–016：主步骤 01–06 的静态审计基础。
- R105.017：`libiap2client.so.1` 的 `iap2_connect` 先 `open(path)` 再 `MsgSendv`；`iap2_raw_msg_send` 也走 QNX MsgSendv，不是直接 RFCOMM socket write。0x9999/0x3000 的服务端语义未闭环。
- R105.018：`mm-ipod` 真实注册 QNX Resource Manager endpoint，配置 io_open/io_read/io_devctl 等 handler。
- R105.019：`io_msg` 组装 QNX 消息后转给 device context `+0x24` callback；该 callback 的写入者尚未定位。
- R105.020：`ipod_main` 顺序为 mount → ACP init → driver init → transport init；`device_transport_init` 通过 `iface_find("Transport")` 取插件并调用 iface `+0x14` 初始化。具体无线插件实例仍未知。
- 下一条最短静态路线：找到 context `+0x24` callback 的初始化写入点，解析 0x9999/0x3000，再追到 `ipod-drvr-iap2.so` transport send/receive 和外层 Transport producer。

### C. AirPlay / Wi-Fi 数据面
- R86：原厂 DIO → libairplay server/session delegate 两级注册结构已 SHA 固定；60/56-byte delegate layout 和 callback slots 已静态确认。
- R105.021：修正旧依赖状态；`libcomm.so`、`libutil.so`、`libosal.so`、`libiplcommon.so` 已有哈希/加载器证据，不得重新列为缺失；`libirc_mmx_adapter.so` 也已从 ImageFS 提取。ASI IapDeviceServices raw bridge 已关闭。
- R105.022：复用公开 MHI2Q Wi-Fi CarPlay 参考架构，提取 Bluetooth bootstrap → Wi-Fi AirPlay → iAP2 inside AirPlay session；禁止照搬 QC 的地址、接口名、GOT/offset。
- R105.023：梳理 Event Channel 单读者/无 CSeq wire-order 风险与 Bluetooth bootstrap gap。
- R105.024：复用 R86 delegate 证据；真实 session callback 是 `s_handleSessionControl @ 0x1594EC`，不再重新寻找 delegate owner。
- R105.025：该 callback 将 CFLString/CFLDictionary 送入事件构造路径并等待结果；自身未见直接解码 iAPSendMessage。
- R105.026：`0x159598` 是 event receiver 暂存/转交候选，tail-call `0x16DBE4`；函数语义和字段 schema 未完成。
- 下一条最短静态路线：只分析 `0x16DBE4` 的函数边界、参数流、读写字段、下游 handler 和返回路径；确认后再决定是否接入 inner_airplay。不要重扫 R86 delegate。

### D. SI/DIO 与恢复
- R71–R85：SI/DIO 单 owner、session epoch、媒体证据、Unix peer admission、START/STOP、wired takeover 和恢复屏障已存在源码/host tests；OEM 生产 callback 仍未绑定。
- `inner_iap2.c`、`inner_socket.c`、`inner_airplay.c`、`cprx_service.c` 是可复用实现，不要另起一套 parser/IPC/lifecycle。
- `inner_airplay` 的 host fake CFL 测试只验证适配器逻辑，不等于 OEM AirPlay callback 已经接入。

### E. R95/R96 离线安装事务
- R95：步骤 3–17 的 15/15 host contract tests 通过；若干使用 OEM/MFi mock。C transaction `trial/recover`、SHA/权限清单和 11 个边界/负面测试通过。
- R96：主机 oneclick/rollback harness 与确定性 review ZIP 已完成；这不是车机生产 installer。生产进程 stop/start、真实 QNX 文件系统、启动健康检查和 boot recovery 未验证。
- R95/R96 是旧离线计划与工具成果，可复用，不作为当前 22 步无线功能完成数。

## 3. 实车已有证据（仅用来界定边界）

R106 用户上传日志已分析：
- QNX 6.5 / ARM / Tegra T30。
- `btstack`、`bluetooth`、`smartphone_integrator` 进程运行；不等于 iAP2 RFCOMM 注册成功。
- `uap0` AP channel 11 / 2.4 GHz active，并出现 station association；不等于 AirPlay 或 CarPlay 会话成功。
- USB `mm-ipod -d iap2 -t usbdevice` 启动；`carplay0`、DIO AirPlay `_airplay._tcp`/5000、SI `CARPLAYVERIFIED`、视频 READY、音频播放均有强证据。这证明原厂有线 CarPlay 路径，不证明无线路径。
- R106 OEM file copy 返回 0 个文件；不能据此说车机没有 OEM 文件。此前已上传的 app.img/固件/原厂 ELF 应优先复用。
- 5GHz 硬件能力未知；保留“5GHz 优先、不可用回退 2.4GHz”的设计目标，但不得宣称硬件已支持。

## 4. GitHub 能做与不能做的测试边界

### 统一放到 GitHub Actions（不要求上车）
1. C/C++ strict compile、静态链接/未解析符号检查、ELF class/arch/ABI 检查。
2. 锁版本 QNX ARM cross compile/link；产物必须标记 QNX-target-built，不等于在车机运行。
3. Rust iAP2 LinkEngine tests、C RFCOMM framing、iAP2 SYN/LSP/ACK 状态机测试。
4. 模拟 BlueSDK/BT owner callback 的协议和生命周期测试（明确 HOST MOCK）。
5. MFi 消息顺序/状态机 mock、拒绝路径、错误证书/超时测试；不能生成或伪造 Apple/OEM 凭据，也不能声称真 MFi 通过。
6. Wi-Fi handover、2.4/5GHz capability policy、mDNS/DNS-SD、IP/端口发现模拟。
7. AirPlay command envelope、CFDictionary/CFData fake adapter、event channel 单 reader、无 CSeq wire-order、背压/partial I/O/timeout/drop/reconnect 测试。
8. SI/DIO AF_UNIX peer admission、generation/epoch/session、START/STOP、wired priority、rollback barrier 测试。
9. SD manifest、SHA256、权限/路径穿越/符号链接检查、host trial/recover/boot-failure rollback 测试。
10. ZIP 确定性、artifact manifest、SHA256、NO_INSTALL 标记和发布门禁。

### 必须保留为真实设备/真实 iPhone 验收
1. QNX 6.5 目标上实际加载/运行的 ABI、进程 owner、线程/回调、socket 与 Resource Manager 行为。
2. BlueSDK 真实 RFCOMM 注册、iPhone 配对、真实 iAP2 握手和真实 OEM MFi 授权。
3. 车机 AP 与 iPhone 的真实 association、DHCP/IP/Bonjour 发现、AirPlay session 建立。
4. 真正 iAPSendMessage 双向数据、真实音视频、Siri/方向盘/MMI/仪表输出。
5. USB 与无线互斥、断线/重启/恢复、真实 SD 写入与车机开机健康检查。

GitHub 不能模拟真实 RF、Apple 设备认证、车机硬件、真实 QNX kernel/device drivers、真实 MFi credentials。QEMU/host mock 通过不得替代这些验收。

## 5. 当前 GitHub CI 状态（截至本次整理）

分支 HEAD：`2f5adce342796b7e8beeb122804cf730a373f588`。

- Unified parallel development run `45808426661`：SUCCESS。包含 QNX ARM toolchain build、SI/CPRX ARM compile/link、6 个 host lanes（radio/media/stock-config/recovery/transport/authentication）、upstream graft 7/7、review artifact collection。
- SI real CPRX integration run `45808426598`：SUCCESS。包含真实源码构建和 host lifecycle/callback 测试；其中 OEM callback 使用测试夹具，不能视为车机运行。
- R105 host regression run `45805562311`：SUCCESS。Rust upstream tests、C framing、owner nonblocking gate、Step01–06 metadata gates、AirPlay envelope host fake test 均通过。
- 早先 run `45803795533` 曾失败，原因是新增 shell command 的续行写法；修复后 `45805562311` 对应 workflow 已 SUCCESS。以后以修复后的 run 为准，不再重复跑旧失败版本。
- 当前 branch HEAD 最新统一任务 run `45808426661` 和 SI run `45808426598` 均成功；报告文档提交不触发所有 workflow，除非匹配其 path filter。

### CI 去重规则
- 仅在相关源文件、构建脚本、工具链锁定版本或测试定义变更时运行对应 workflow。
- 纯 Markdown 文档更新不跑编译和全量 host suites。
- 仅测试脚本变更时运行该测试 lane，不重跑无关 QNX toolchain build。
- 共享基础库变更才运行受影响的所有依赖 lane；release candidate 才跑一次全量矩阵。
- CI artifact 带 commit SHA、toolchain SHA、source SHA、测试名称、HOST_MOCK/NO_INSTALL 范围标签。
- 同一 commit 同一 workflow 的 SUCCESS 可复用；不得为了“给用户看”反复重跑。

## 6. 研发防重复规则（必须执行）

1. 每个发现写入本文件或其对应专项报告，并记录原厂文件 SHA、函数 VA、证据级别、CI run ID。
2. 状态分四类：`STATIC_CONFIRMED`、`HOST_TESTED`、`QNX_ARM_BUILT`、`VEHICLE_VERIFIED`。禁止把前三级合并成最后一级。
3. 对已 `STATIC_CONFIRMED` 的同一 ELF 哈希，不重扫同一函数/同一 relocation；只在新证据冲突时复核。
4. 对已 `HOST_TESTED` 的同一 commit/test，不重复运行；代码变化只跑受影响 tests。
5. 对已 `QNX_ARM_BUILT` 的同一源码和 toolchain SHA，不重复下载/重编；除非构建输入变化或 artifact 丢失。
6. 不再向用户索取已经上传且哈希一致的 app.img、dio_manager、libairplay、mm-ipod、ipod-drvr-iap2、libiap2client、btstack 等文件。
7. 只有确实缺失且不能从已交付固件/交接包恢复的精确文件，才一次性列出路径、文件名、所在镜像和用途。
8. 每轮必须先提交真实源码/测试/构建改动；如果只做静态研究，要明确说没有新增功能代码，不把报告篇数当进度。
9. 任何主机 mock 必须在报告和 CI 名称中标注 HOST MOCK；不允许让 mock 伪造 OEM owner/MFi/media witness 后触发生产成功状态。
10. 安装包始终 fail-closed：没有生产 owner callback、真实恢复屏障、文件路径/哈希和 QNX 启动健康证明前，`installable=false`、`safe_to_flash=false`。

## 7. 当前唯一下一任务（整理结束后的第一个开发任务）

**R105.027：只拆解 MU1005 原厂 dio_manager VA `0x16DBE4`。**

交付必须是：
- 函数边界/大小/调用约定；
- 输入参数和从 event 暂存区读取的字段；
- 关键下游 call graph；
- CFLString/CFLDictionary/response dictionary 的实际读写；
- 是否能证明它处理 AirPlay control 命令，是否包含 iAPSendMessage 分支；
- 如果不包含，继续追踪真实 iAP2 数据通道，不要强行把控制事件当 raw transport。

本任务不重新跑 R86 delegate audit、不重跑 R105.017–020 已确认的 MsgSendv/ResMgr/loader 静态检查、不重复跑已通过 CI。只有新增 adapter/test code 后，才触发相关 GitHub workflow。

## 8. 当前状态（不可越界）

`installable=false`  
`safe_to_flash=false`  
`vehicle_verified=false`  
`mfi_apple_verified=false`  
`wireless_carplay_e2e=false`

本文件整理完成不代表研发继续已获授权；用户明确要求先整理好再继续。后续先以本文件作为交接依据，等待用户确认后再开展 R105.027。