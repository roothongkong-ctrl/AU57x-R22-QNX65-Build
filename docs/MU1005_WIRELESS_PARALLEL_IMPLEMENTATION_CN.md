# MU1005 纯软件无线 CarPlay 并行实施计划（NO INSTALL）

基线：2017 Audi A6L C7，MHI2_CN_AU57x_K1006_2 / MU1005，QNX 6.5 ARMv7。
执行分支：mu1005-wireless-implementation-r103-20261125
基线提交：7f595231394d97091ec0fee209677b6e45fd50dc

> 本计划是源码研发与云端验证计划，不是可安装包。禁止将主机 fixture、QEMU 合成进程、静态符号或接口契约表述为真实 iPhone / MFi / 车机无线链路成功。原厂有线 CarPlay 必须保持优先，任何原厂进程/接口的生产绑定未得到证据前均 fail closed。

## 一、当前已有证据（来自仓库现有记录）

- R29：保存样本中 bluetooth 与 btstack 的静态符号/字符串标记 7/7 符合；证明静态对象候选存在，不证明实际 DSI 注册、RFCOMM 回调归属或 iAP2 数据流。
- R41/R44：已有原厂 libairplay.so MFi 证书/签名导出符号静态证据，以及 AA00–AA05 的软件状态机与 RFCOMM→iAP2→MFi 接线；主机使用模拟符号，未证明真实 iPhone MFi 成功。
- R43/R45：已有受限 iAP2 链路层移植和 MU1005 BlueSDK 原生模块静态复核；真实 BlueSDK callback owner、mm-ipod/iAP2 运行时接线仍未证明。
- R50：已有无线控制、AirPlay listener/Bonjour、SI owner、媒体状态等组合模型；真实 OEM 回调仍需绑定。
- R71–R85：已有 SI/DIO 分进程、AF_UNIX CPRX、同会话媒体证据、有线抢占与恢复协议；关键 SI/DIO 生产回调/进程身份 ABI 尚未在原厂二进制中定位。
- R86：已有对原厂 dio_manager 调用 AirPlayReceiverServerSetDelegate / AirPlayReceiverSessionSetDelegate 的两级回调 ABI 静态还原，确定服务器回调结构 60 字节、会话回调结构 56 字节及 21 个真实 GOT 回调槽。它为只读观察层提供事实依据，但仍未证明真实运行回调触发和媒体/MFi结果。\n- R100：已有原厂路线 / 可回滚 graft 双路线门禁；原厂服务优先，缺失真实 endpoint、BlueSDK owner、iAP2 注册参数、原厂退出/恢复证明时不得 graft。\n- R103：已有 LuKa 上游 iAP2 LinkEngine 的 Rust RFCOMM 帧桥，C 主机 45/45 通过；Rust/TCP 测试已在本分支 GitHub Actions 45633825725 SUCCESS：上游 7/7、bridge 2/2、TCP lab 13/13，共 22 项 Rust host 测试通过；C bridge test 通过。Rust QNX ARM 目标仍未构建。\n- R106/001：已有实车日志证据，原厂 uap0 2.4GHz AP（信道 11）启动并出现 STA_ASSOC；原厂有线 USB iAP2→dio_manager→AirPlay Receiver→carplay0 的 READY 证据。没有无线 iAP2/MFi/AirPlay 会话证据。\n- 5GHz：已有 uaputl、Marvell 固件和 WLAN 驱动静态证据，软件工具/驱动含 A-band 处理代码；connectionmanager 当前 AP 模板默认仅 1/6/11。无线固件实际加载版本、fw_bands 运行值、主板 PA/滤波器/射频通路均未证实。\n- R143：单独的 QNX 观察器/线程归属研究分支。QNX ARM 编译通过，但 QEMU 合成测试报告 /proc/boot/btstack 打不开（errno=2）；需要先确认 IFS 文件映射/测试执行路径。此观察器不是无线 CarPlay 的核心链路，不能阻塞其他模块并行研发。

## 二、执行策略：先做原厂能力边界，再并行推进

### Track A — 原厂能力与硬件边界（先行审计）
1. 复核原厂 bluetooth、btstack、smartphone_integrator、dio_manager、libairplay.so、libiap2client.so.1、mm-ipod/iAP2 插件的样本版本、SHA256、ELF NEEDED、导出符号与静态调用边界。
2. 以 R106/001 实车快照为基准确认 uap0 2.4GHz AP 已启动且发生客户端关联；另复核 uaputl、sd8787_uapsta.bin、w8787_wlan_SDIO_bt_SDIO.bin、connectionmanager 与两版 mrvl_wlan 驱动。5GHz 只确认存在软件处理路径，实际加载固件/射频前端仍未知。
3. 输出“已证实 / 推断 / 未知 / 需要实车”的能力矩阵。当前基线为：2.4GHz AP 有实车快照证据；5GHz 仅静态软件证据；iAP2 over Bluetooth、MFi、无线 AirPlay 仍未证实。

### Track B — Bluetooth / RFCOMM / iAP2
1. 将 R39–R45 规范化 RFCOMM 队列、BlueSDK 适配与 iAP2 link/auth 组合为单一 owner/epoch/generation 接口。
2. 分别验证帧重组、ACK/重传、断链、重复包、会话代际变化、原厂有线抢占。
3. 只有找到原厂真实注册/回调 owner 与 mm-ipod/iAP2 消费端后，才允许做生产绑定；否则保持 host/QEMU adapter-only。

### Track C — 原厂 MFi
1. 复核 R41 原厂 libairplay.so 句柄来源、ABI、内存释放函数及 owner 线程约束。
2. R42/R44 的 AA00–AA05 仅作为协议状态机；测试桩明确隔离在 host test binary。
3. 真实 MFi 成功必须有原厂可信 peer_auth_succeeded、同一设备/会话/代际和真实 iPhone 事务证据。禁止自制签名、伪造证书或绕过 MFi。

### Track D — Wi-Fi / AirPlay
1. 复核 R50 的无线电策略、Wi-Fi interface discovery、AirPlay listener、Bonjour identity 与 session 生命周期。
2. 将 2.4/5GHz 选择实现为能力探测驱动：先以已观察到的 2.4GHz AP 为基础连通路径；仅在实际无线固件/驱动报告支持且硬件射频通路有证据时启用 5GHz。失败回退必须验证关联、地址、路由和链路状态。
3. 验证 AirPlay 发现、会话协商、密钥/认证、音视频 transport 与 teardown；不以 listener bind 成功代替 AirPlay session 成功。

### Track E — SI / DIO / 原厂媒体与有线恢复
1. 延续 R71–R85 的分进程架构，不在 SI 进程持有 DIO 指针。
2. 查明真实 SI/DIO 进程身份、AF_UNIX peer admission、START/STOP、媒体状态与原厂 restore 回调。
3. 所有媒体 ACTIVE 必须由同一 SI session 的真实 DIO/iAP2/MFi/video/audio/MMI/steering 观察证明。
4. 有线接管最高优先级；必须等原厂 DIO restore 与 SI teardown 双确认后才报告恢复成功。

## 三、统一验收门禁

- H0 源码/许可证/来源与样本哈希可追溯。
- H1 Linux host：GCC/Clang、ASan/UBSan、fuzz、socket/process-boundary tests。
- H2 QNX 6.5 ARMv7：真实目标 ELF、NEEDED/符号/ABI 检查。
- H3 QEMU QNX：加载、运行、错误路径、退出码和日志归档。
- H4 OEM runtime binding：原厂进程、线程、owner、会话和资源释放证据。
- H5 真 iPhone/MFi/Wi-Fi/AirPlay 端到端验证。
- H6 车机有线优先、重连、故障恢复和回滚验收。

H0–H3 通过只代表云端源码/仿真验证，不代表 H4–H6。未通过 H4–H6 前，始终 NO_INSTALL、safe_to_flash=false。

## 四、当前执行顺序

1. R143 QEMU 预检继续单独修复，但不阻塞其他轨道。
2. Track A 输出只读能力矩阵，并把 R86 的两级 DIO 回调 ABI 与 R100 的原厂/graft 双路线门禁纳入正式接口图。
3. Track B/C 合并 iAP2 与原厂 MFi 的单 owner 契约，补齐失败/重连/有线抢占测试。
4. Track D 对 Wi-Fi 频段能力、接口选择、AirPlay 会话边界做独立测试。
5. Track E 补齐 SI/DIO 生产 callback 的证据清单；缺口未证实则不做虚假接线。
6. 最后统一集成、构建和只读研究包；绝不把研究包伪装成安装包。

## 云端验证记录（2026-11-25）

- R103 上游 iAP2 LinkEngine + AU57x RFCOMM bridge：GitHub Actions 45634439847 SUCCESS；原始上游测试 7/7、bridge 测试 2/2、TCP lab 13/13，共 22 项 Rust host 测试通过；不代表 QNX Rust 或 OEM 蓝牙运行。
- 全量 cloud12 review：GitHub Actions 45634439831 SUCCESS；包含 45/45 C host strict compile、R86 delegate ABI parser negative tests、R100 17 条 route gate assertions、R101 13/13 offline suites、R95 15/15 offline matrix、R96/R97/R98/R99 regressions、50,000 CSM fuzz、67 native SD transaction tests；全部为 host/静态研究，不是车机运行。
- R52 QNX 6.5 ARMv7：45632464697 曾成功构建 GCC 8.5 工具链，但 compile script 内部仍写 44（而 workflow preflight 已是 45），在正式 C 编译前退出。已将 `ci/compile_qnx_in_image.sh` 修正为 45；新任务 45633825933 已通过 45 源码预检，目前正在构建工具链，QNX C 编译、ELF gate 与 QEMU 尚未返回。
- 独立 R143 QEMU 观察器：45629972898 已确认 QNX ARM32 编译 PASS，但合成进程的 /proc/boot/btstack 打开失败（errno=2）；该问题与 R103 iAP2 host 测试独立，暂不阻塞并行模块。

## 五、首轮并行任务状态

| 轨道 | 首轮任务 | 状态 |
|---|---|---|
| A | 原厂 ELF/网络/无线能力只读审计 | 进行中 |
| B | RFCOMM→iAP2 owner 契约核对 | 待本分支验证 |
| C | R41/R44 原厂 MFi 调用边界核对 | 待本分支验证 |
| D | Wi-Fi 双频能力证据与回退策略核对 | 待本分支验证 |
| E | R81–R85 SI/DIO 恢复链生产回调缺口核对 | 待本分支验证 |
| R143 | QEMU 原厂样本路径预检 | 独立运行中 |

最后更新：2026-11-25。
## 第二轮并行计划与当前提交（2026-11-25）

1. **Track D / Wi-Fi interface resolution（云端可完成）**：新增 `tests/test_wifi_interface.c` 与 `ci/host_wifi_interface.sh`，验证非法 IPv4、loopback、未分配地址、真实 up/non-loopback IPv4 唯一映射及输出缓冲区边界；已接入 cloud12 workflow。该测试只验证地址到接口名解析，不宣称 Wi-Fi AP、频段、iPhone 或 AirPlay 已工作。
2. **R52 QNX build（本轮已通过）**：45 个 C 文件基线、QNX C shared link、ELF gate 和 QEMU dlopen smoke 已通过；下一步是将真正可复用的 donor daemon/graft 组件分别交叉编译，而不是把这次 C 工程成功当作完整无线系统成功。
3. **Track B/C（下一并行批）**：基于已通过的 R103 host link bridge，审查 QNX 端 Rust target/标准库可用性；若 QNX Rust 不可行，评估 C ABI 边界/独立 Rust host daemon 的可行性，不先假定可移植。同步审查原厂 MFi API 句柄/内存释放/线程契约，所有模拟认证继续标为 fixture。
4. **Track E（后续）**：只读核对 SI/DIO callback、peer identity 与 wired restore 证据；生产绑定未证明前保持 fail-closed。

本轮新增 Wi-Fi resolver 测试不改变运行时配置，不调用 OEM API，不生成安装文件。

## 第三轮：成熟项目复用与最新验收（2026-11-25 14:37 UTC）

### 外部成熟项目核对

- `congobg/MHI2Q_WiFi_CarPlay` 当前公开状态文档报告 MHI2Q QC / QNX 6.5 ARMv7、固件 3638 与 5092 上无线 CarPlay 实车运行，复用 stock btstack、stock iAP2 client、stock AirPlay receiver、stock HMI；5092 的启动、蓝牙配对/iAP2、Wi-Fi、Bonjour、媒体和长时间会话均有项目方记录。它是重要的流程与架构参考，不是 AU57x 安装包。
- 该项目明确目标为 MHI2Q QC；AU57x 的 MHI2_CN_K1006_2/MU1005 不是同一固件/二进制基线。禁止复制 donor ELF/JAR/脚本直接装车。此前用户提供的 donor ZIP 已完成 8 个组件的文件名、大小、SHA256 和 ELF 类型审计；仅保留参考清单，没有把 donor 二进制放进本工程。
- `luka-dev/mib2q-carplay-rgi` 的 iAP2 link 源码已按原始 Git blob 校验后纳入 `third_party/`，遵守 GPLv3 来源要求；R103 host CI 已通过。其上游 MHI2Q 固定地址不得用于 AU57x；本工程仅复用协议层/通用源码，OEM 绑定必须按 MU1005 原厂证据重新定位。

### 最新云端验收（GitHub Actions）

- R52 QNX6.5 ARMv7：run `45634440000` SUCCESS。45 个 C 源码预检、QNX GCC 8.5 工具链构建、native compile/shared link、R96/R92/R100/R101 QNX 对象/共享库、严格 ELF gate、QEMU QNX dlopen smoke 全部通过。生成的 `libau57x_r24_native_qnx_REVIEW_ONLY.so` 与 `libau57x_bt_preload_qnx_REVIEW_ONLY.so` 是 REVIEW ONLY；日志明确 NO_GO for installation，且 QEMU 未调用 OEM 原厂 API。
- cloud12 全量离线回归：run `45634439831` SUCCESS。包含 45/45 host C strict compile、R87 autoconnect 状态机、R90 iAP2 link/fixture MFi 路径、R89 跨进程 socket 契约、R96 事务/回滚、R100 17 条门禁、R101 离线套件、50,000 组 fuzz、Wi-Fi IPv4→接口解析；其中 OEM/MFi/BlueSDK 真实事件仍未证明。
- R103 上游 iAP2：run `45634439847` SUCCESS。上游 7/7、AU57x bridge 2/2、TCP lab 13/13 host tests 通过；不代表 Rust/QNX ARM 已构建，也不代表真实蓝牙/iPhone 连接。

### 下一批实际工作（依赖优先）

1. **优先完成 MU1005 OEM Bluetooth attach 的证据闭环**：R88/R89 已证明跨进程传输和 fail-closed 逻辑，但缺少可安全绑定到真实 btstack 回调 owner/线程的 OEM 运行时证明，以及 iAP2 profile/SCN 的独立证据。先审计 R97 的 pthread/GOT 观察路径、R89 原厂调用点和已有私有 btstack 样本，形成只读 attach 设计；不调用未知 ABI。
2. **并行完成 donor 对照表**：将 congobg 项目的配对、SDP/RFCOMM、Wi-Fi AP/信道、Bonjour、dio_manager、HMI 启动顺序逐项映射到 MU1005；标记“可复用协议源码 / 需重写 OEM 适配 / 不可复用二进制”。
3. **MFi / AirPlay / SI-DIO**：继续使用 R41/R44/R86/R85 既有成果，补齐原厂句柄/回调 owner/会话证据；mock 只作测试 fixture。
4. **多手机管理**：设计 per-device pairing record、last-successful-device 优先、删除单设备清理其蓝牙与无线 CarPlay关联数据；先核对原厂 HMI/BT 配对数据库接口，禁止擅自清空全局目录。
5. **SD 一键安装器**：复用 R95/R96 事务备份、校验和回滚引擎；等 OEM attach、启动入口、配置变更清单和持久路径证据闭环后才生成候选安装包。目标为安装后拔卡可运行，当前仍 NO_INSTALL。

当前结论：云端 QNX ARM compile/link/ELF/QEMU smoke 与离线测试已通过；真实 MU1005 OEM Bluetooth attach、iPhone MFi、无线 AirPlay 媒体链路和整车验证未通过/未进行。状态保持 `RESEARCH_ONLY / NO_INSTALL / safe_to_flash=false / vehicle_verified=false`。
