# 无线 CarPlay｜唯一主交接入口

更新：2026-11-27（本轮文档交接）
研发分支：`mu1005-si-cprx-real-review`
最近核实的功能研发基线：**R105.156 / 88b0ceace9e8da91b03f33188eb61f9271a2cc95**
状态：**SOURCE REVIEW ONLY / NO INSTALL / NOT VEHICLE VERIFIED**

## 1. 新会话先读这里（不依赖旧聊天）

- 用户发“1”或“继续”：按本文件最新下一任务接续研发，不重做已完成工作。新会话首次先读取本文件、实际分支 HEAD；若 HEAD 已推进，只检查本基线之后的差异及相关 Actions 结果，不能回退到旧版本。
- 目标：2017 Audi A6L C7 PA，MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32，纯软件无线 CarPlay，保留有线，蓝牙引导 + Wi-Fi，5GHz 优先、2.4GHz 回退，不绕过 MFi。先打通首连，再优化；不与仪表地图项目混合。
- MIB Toolbox 3.8、SSH、原厂蓝牙及热点已有。5GHz 真实硬件能力未知。最终 SD 安装、备份、校验、回滚需真实证据；目前禁止安装。
- 项目事实以 GitHub 文件、提交、Actions 原始日志、明确来源的实车证据为准，不以聊天摘要或主观百分比为验收依据。

## 2. 当前状态和下一任务（R105.156 → 后续）

**下一任务 R105.157：停止重复配置 reader 静态扫描，将 R105.155 只读实车探测列为待办，优先推进真实 RFCOMM/iAP2 原厂接入证据。** 已通过 GitHub repository Actions runs GET 查到 R105.156 run 45998478475，两个 job 均 SUCCESS；原始日志分别明确打印 R105156_EVIDENCE_ONLY_NOT_CONSUMER_VERIFIED。reader-targets 解码两个 callee 但没有证明配置 consumer；boot-path 扫描201个文本文件，找到启动脚本路径候选，未证明实车挂载或生效。保留首连配置第03步90%，不重跑此测试。

- R105.156：[报告](R105156_CONFIG_READER_CN.md)。Bluetooth enableIap 已知调用链继续追踪 0x15619c → 0x21d814 / 0x21cc68，另核对 stage2 启动路径，避免再次全量 marker/ABI/NAS 扫描。
- 若两路提供新 reader/路径证据，记录实际结果及仍缺的运行时生效证明；若无新证据，停止同类反复扫描，将只读配置路径、真实 reader、生效和受控回滚验证合并为一次实车探测待办，转向 RFCOMM/iAP2 真实接入、原厂 MFi、AP owner、AirPlay 首连。不允许以静态候选代替运行证据。
- “首连配置第03步”按 R105.156 报告仍为 90%（研发估算，不是实车成功率）；它不是 22 步主表第03步 mm-ipod loader，不能混合编号。
- 当前无已验证的真实无线 iPhone 端到端连接，无生产安装包。

## 3. 最近关键证据（完整报告按需打开）

| 版本 | 已知结果及范围 | 证据 |
|---|---|---|
| R105.144/145 | 15 路静态候选检查 11/15；下一调用链及失败搜索已记录；不是功能测试 | [144](R105144_15_FIRST_LINK_CN.md) / [145](R105145_FIRST_LINK_PATHS_CN.md) |
| R105.149/150 | 8/8 及累计 12/12 host socket fixture；真实 TCP/Unix socket，radio/MFi/DIO witness 为模拟 | [149](R105149_LIVE_AP_CHAIN_CN.md) / [150](R105150_LIVE_AP_ORDER_CN.md) |
| R105.152 | 原始 QNX job 140483784256 有双频回退标记及 exit0，workflow 曾红；artifact 15194312845；非真实 RF | [152](R105152_QNX_LIVE_AP_CN.md) / [154](R105154_CI_EVIDENCE_CN.md) |
| R105.153 | 4/4 新增及 12/12 回归通过（仓库报告）；热点控制权仍需 OEM provider | [153](R105153_AP_OWNER_CN.md) |
| R105.154 | R105.156 报告记录 31/31 原厂直接依赖在 fork 子进程 dlopen NOW 成功；旧 R105.151 31 次 SPAWN_FAIL 不算成功；不是 OEM 服务正常运行 | [154](R105154_CI_EVIDENCE_CN.md) / [156](R105156_CONFIG_READER_CN.md) |
| R105.155 | 已直接查询 GitHub：host-fixture 和 qnx-read-only-probe 两项成功；报告记载 probe exit0，artifact 15196840612；未在实车运行 | [报告](R105155_CONFIG_PATH_CN.md) / [run 45995760126](https://github.com/roothongkong-ctrl/AU57x-R22-QNX65-Build/actions/runs/45995760126) |
| R105.156 | 实际 run 45998478475 两路 SUCCESS，job 140503777308 / 140503777888，artifact 15196748262 / 15197356418；静态候选证据，非真实 reader 生效 | [报告](R105156_CONFIG_READER_CN.md) / [commit](https://github.com/roothongkong-ctrl/AU57x-R22-QNX65-Build/commit/88b0ceace9e8da91b03f33188eb61f9271a2cc95) |

R106 实车已有：原厂进程、2.4GHz AP association、有线 CarPlay 视频音频等证据；没有真实无线 CarPlay、真实无线 MFi、真实 5GHz 的成功证据。

最新通用 SI/CPRX workflow run 46001009254 SUCCESS（文档 HEAD 6c912abd）；两次纯文档提交仍触发该 workflow，后续应核对触发过滤器，不能说已避免无关 CI。

## 4. 固定22步主表及历史资料

保持原22步编号。原交接只明确列出 01–06，07–22 尚未逐项映射，不虚构完整表或进度。
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

上表为 2026-11-26 历史静态基线，不能当作当前逐项重新验收结果。后续依据新增证据更新正式表，不按研发报告编号计算完成数。

[上一版完整主交接（固定历史快照）](https://github.com/roothongkong-ctrl/AU57x-R22-QNX65-Build/blob/88b0ceace9e8da91b03f33188eb61f9271a2cc95/docs/MASTER_HANDOFF_AND_NO_REPEAT_RULES_CN.md)：包含 R45–R106、上游 LinkEngine、mm-ipod、ASI、SI/DIO、AirPlay、恢复事务、历史 CI 和防重复规则。旧版 R105.027 下一任务已过时，不再执行。专项报告均在仓库中按需读取，不搬入聊天。

## 5. 每轮研发必须同步 GitHub（减少会话长度）

- 实际代码、测试、构建脚本、技术报告、CI 与可复用产物放仓库/Actions；聊天只汇报本轮关键变化、真实结果、阻塞、下一任务及链接。不要整段贴反汇编、完整日志、历史长表。
- 每轮完成一个有效研发增量或拿到关键新结果后，**在回复用户前更新本主交接文件**：功能基线 SHA、版本、证据等级、run/job/artifact ID、当前阻塞和唯一下一任务。使用最新 blob SHA 防止覆盖并发修改；冲突先重读合并。纯文档提交不应重复触发无关 CI。
- 主交接文件只保留当前快照和简明索引；长技术历史放专项报告和 Git 历史，不能每轮无上限追加。Actions artifact 可能过期：重要的脱敏结果与摘要长期写入仓库；原厂专有固件、密钥、账号、私人日志不得提交到公开仓库。
- 独立工作可并行约15路、最多20路；以有效任务为准，不凑数量。已通过的同输入 CI 直接复用，除非相关输入变更、证据冲突或产物确实丢失。优先复用成熟 GitHub 源码及已有原厂文件；确实缺失才一次性列出路径/镜像/用途。
- 区分 STATIC_CONFIRMED、HOST_TESTED、QNX_ARM_BUILT、QNX_GUEST_EXECUTED、VEHICLE_VERIFIED。编译、模拟或加载通过不等于真实服务运行，更不等于真实 iPhone 无线连接。保留原厂有线优先及 MFi；不将未知硬件能力写成已验证。
- 安装门禁：installable=false；safe_to_flash=false；vehicle_verified=false；mfi_apple_verified=false；wireless_carplay_e2e=false。未取得真实首连和恢复证据前保持 NO INSTALL。
- GitHub 写入失败或无法读取真实结果必须明确告知用户，不能宣称已同步/已通过。不会依赖会话即将耗尽时才交接，也不保证未运行的会话自动更新。

## 6. 用户新对话只需发送

> 继续无线 CarPlay 研发。读取 GitHub 仓库 roothongkong-ctrl/AU57x-R22-QNX65-Build，分支 mu1005-si-cprx-real-review 的 docs/MASTER_HANDOFF_AND_NO_REPEAT_RULES_CN.md，核实最新 HEAD 和相关测试，按最新下一任务继续；每轮更新交接文档，不重复研究。

## 7. 用户确认的简洁会话模式（2026-11-27）

日常研发以 GitHub 源码、Actions 和交接文档为工作中心；真实硬件/iPhone 验证仍须实车。每轮默认仅回复一张简洁进度表（研发版本、实际完成项、测试结果、阻塞、下一任务、同步状态），不重复解释背景、不贴完整日志。用户发 1/继续 即按当前任务执行；只有需要讨论实质技术方案或重大取舍时才用简短文字讨论，方案明确后执行。每轮更新主交接文件并核实写入；没有实际执行的事项不得写成已完成。

## 8. 固定10项用户进度表：2026-11-27 证据复核版

这是用户展示层，原22步技术编号及原首连专项编号保留。废止此前没有明确分母的10项成熟度百分比和63%综合估算。每项5个明确证据验收点，每通过一个计20%；这是等权检查点覆盖比例，不是剩余工期、功能完成率、实车成功率。以下为本轮基于现有证据制定的计分口径，不是历史既有标准。未核实的结果不计分。每轮仅凭新证据更新相应检查点，不凭提交数、CI绿色或主观判断加分。

| 项 | 已有证据（计分） | 尚未验收（不计分） | 计分 |
|---|---|---|---|
| 01 原厂配置读取与生效 | 精确原厂单字段候选；副本 apply/restore；QNX只读检测工具运行 | 实车有效文件及reader；原厂实际生效 | 3/5 |
| 02 蓝牙 RFCOMM | 原厂接口静态边界；软件桥接及owner lifecycle fixture | 原厂iAP2专属注册/owner/SCN；真实callback双向接入；iPhone实际RFCOMM | 2/5 |
| 03 无线 iAP2 | 上游LinkEngine集成；同socket SYN/LSP/ACK fixture | 原厂driver/transport运行接线；真实无线字节双向交付；iPhone真实握手 | 2/5 |
| 04 原厂 MFi | 认证状态机及拒绝路径实现；模拟认证联合测试 | 原厂认证provider绑定；真实iPhone认证交换；原厂认可认证成功 | 2/5 |
| 05 Wi-Fi热点连接 | R106原厂2.4G关联证据；AP适配器与5703 fixture；QNX ARM AP对象构建 | 原厂AP owner实际绑定；真实iPhone无线引导后关联及IP可达 | 3/5 |
| 06 双频切换 | 5G优先/2.4G回退策略；host双向回退；QNX guest fixture实际退出0且保留artifact复核 | 真实5G AP硬件能力；真实手机双频回退 | 3/5 |
| 07 Bonjour | R106有线AirPlay服务发现；联合fixture发现流程 | 原厂DNS-SD daemon真实调用成功；真实无线接口注册；iPhone发现对应无线服务 | 2/5 |
| 08 AirPlay | 原厂delegate静态分析；实际TCP/CPRX Unix socket联合fixture | OEM receiver真实接入；真实无线AirPlay session；真实内层iAP2/认证见证 | 2/5 |
| 09 实车无线首连 | 无真实端到端无线验收 | 真实无线BT/iAP2；原厂MFi；无线Wi-Fi发现；无线AirPlay；同一真实认证CarPlay session（可立即断开） | 0/5 |
| 10 SD安装恢复 | SHA/权限/安全清单；主机真实文件事务与恢复；确定性源码复核包 | 原厂QNX生产安装及健康/恢复入口；真实SD安装与开机恢复验收 | 3/5 |

证据核对：
- 最新功能基线仍 R105.156；审查时 HEAD b4dfaf47，最近40条Actions记录已检查。
- R105.125 run 45990637047 job 140477269660 原始日志明确 STEP2/3/4/5 FIXTURE PASS NO OEM/NO REAL MFI OR RF。
- R105.147 run 45993057497：8 host + QNX ARM build 全成功；不是原厂AP owner。
- R105.146 run 45988423332：5/5原厂副本实验事务通过。
- R105.154 run 45994101072：四项通过；job 140489030484 原始日志 R105154_RETAINED_QNX_DUAL_BAND_CONFIRMED_FIXTURE_ONLY，确认 R105.152 红色workflow中的有效QNX guest测试证据；另外三个job为实际库加载诊断，不能等同原厂进程运行。
- R105.118 报告记录 R105.117 原厂Bonjour实际调用 socket errno247 / DNSServiceCreateConnection -65539，未连daemon；不可计真实Bonjour成功。
- R96_INSTALL_LIMITS_CN.md / R96_STEPS_18_19_CN.md 明确仅主机事务及源码复核包。
- 最新通用集成 run 46002056817 四个host jobs成功；没有新功能提交。当前第01项云端证据收集结束，剩余真实reader/生效归入后续受控实车检测；按第02项推进 R105.157，不重复静态扫描。保持NO INSTALL。


## 9. 用户最新确认：第一阶段只求真实无线首连（优先级最高，2026-11-27）

用户明确要求先实现连接，其他功能暂不管。唯一首阶段成功判据：真实 iPhone 在未接USB情况下，经车机蓝牙/iAP2、原厂认证、Wi-Fi、服务发现及 AirPlay 建立同一真实 CarPlay 会话，即使随后立即断开也记录首连成功；单独蓝牙配对、Wi-Fi关联或模拟测试不算成功。不要求视频首帧、完整媒体、音频、Siri、方向盘、仪表、UI、多手机、自动重连、长期稳定性。优先已知实车2.4GHz路径；5GHz优先与双频优化仍是后续目标，不阻塞首连。保留原厂有线与真实MFi认证；受控实验仍需最小安全备份/恢复，不因简化目标而绕过安装门禁。完整SD一键交付后置。

原10项固定编号及证据验收进度保留，不重新计分或凭缩小范围虚增百分比。第01–05、07–09为首连主线，第06及第10的完整功能后置（第10最小必要恢复仍保留）。每次只做直接推进真实首连的增量；当前第01项云端静态研究已收口，实车验证待办，下一研发 R105.157 集中第02项原厂RFCOMM/iAP2接入。以后报告继续使用简洁10项进度条并标注后置项，无需重复讨论非首连功能。
