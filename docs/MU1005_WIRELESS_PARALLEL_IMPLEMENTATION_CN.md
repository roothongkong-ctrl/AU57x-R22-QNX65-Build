# MU1005 纯软件无线 CarPlay 并行实施计划（NO INSTALL）

基线：2017 Audi A6L C7，MHI2_CN_AU57x_K1006_2 / MU1005，QNX 6.5 ARMv7。
执行分支：mu1005-wireless-integration-20261125
基线提交：7f595231394d97091ec0fee209677b6e45fd50dc

> 本计划是源码研发与云端验证计划，不是可安装包。禁止将主机 fixture、QEMU 合成进程、静态符号或接口契约表述为真实 iPhone / MFi / 车机无线链路成功。原厂有线 CarPlay 必须保持优先，任何原厂进程/接口的生产绑定未得到证据前均 fail closed。

## 一、当前已有证据（来自仓库现有记录）

- R29：保存样本中 bluetooth 与 btstack 的静态符号/字符串标记 7/7 符合；证明静态对象候选存在，不证明实际 DSI 注册、RFCOMM 回调归属或 iAP2 数据流。
- R41/R44：已有原厂 libairplay.so MFi 证书/签名导出符号静态证据，以及 AA00–AA05 的软件状态机与 RFCOMM→iAP2→MFi 接线；主机使用模拟符号，未证明真实 iPhone MFi 成功。
- R43/R45：已有受限 iAP2 链路层移植和 MU1005 BlueSDK 原生模块静态复核；真实 BlueSDK callback owner、mm-ipod/iAP2 运行时接线仍未证明。
- R50：已有无线控制、AirPlay listener/Bonjour、SI owner、媒体状态等组合模型；真实 OEM 回调仍需绑定。
- R71–R85：已有 SI/DIO 分进程、AF_UNIX CPRX、同会话媒体证据、有线抢占与恢复协议；关键 SI/DIO 生产回调/进程身份 ABI 尚未在原厂二进制中定位。
- R86：已有对原厂 dio_manager 调用 AirPlayReceiverServerSetDelegate / AirPlayReceiverSessionSetDelegate 的两级回调 ABI 静态还原，确定服务器回调结构 60 字节、会话回调结构 56 字节及 21 个真实 GOT 回调槽。它为只读观察层提供事实依据，但仍未证明真实运行回调触发和媒体/MFi结果。\n- R100：已有原厂路线 / 可回滚 graft 双路线门禁；原厂服务优先，缺失真实 endpoint、BlueSDK owner、iAP2 注册参数、原厂退出/恢复证明时不得 graft。\n- R103：已有 LuKa 上游 iAP2 LinkEngine 的 Rust RFCOMM 帧桥，C 主机 45/45 通过；Rust/TCP 测试虽有历史报告，但构建记录仍标记 PENDING GITHUB CI，必须重新执行确认。Rust QNX ARM 目标未构建。\n- R106/001：已有实车日志证据，原厂 uap0 2.4GHz AP（信道 11）启动并出现 STA_ASSOC；原厂有线 USB iAP2→dio_manager→AirPlay Receiver→carplay0 的 READY 证据。没有无线 iAP2/MFi/AirPlay 会话证据。\n- 5GHz：已有 uaputl、Marvell 固件和 WLAN 驱动静态证据，软件工具/驱动含 A-band 处理代码；connectionmanager 当前 AP 模板默认仅 1/6/11。无线固件实际加载版本、fw_bands 运行值、主板 PA/滤波器/射频通路均未证实。\n- R143：单独的 QNX 观察器/线程归属研究分支。QNX ARM 编译通过，但 QEMU 合成测试报告 /proc/boot/btstack 打不开（errno=2）；需要先确认 IFS 文件映射/测试执行路径。此观察器不是无线 CarPlay 的核心链路，不能阻塞其他模块并行研发。

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