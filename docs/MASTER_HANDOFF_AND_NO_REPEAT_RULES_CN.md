# 无线 CarPlay｜唯一主交接入口

更新：2026-11-27（本轮文档交接）
研发分支：`mu1005-si-cprx-real-review`
最近已验收的功能研发基线：**R105.156 / 88b0ceace9e8da91b03f33188eb61f9271a2cc95**；R105.157 静态测试通过（run 46004940954）；R105.158 四路静态检查及四路集成回归已通过；实际配置读取未闭环
状态：**SOURCE REVIEW ONLY / NO INSTALL / NOT VEHICLE VERIFIED**

## 1. 新会话先读这里（不依赖旧聊天）

- 用户发“1”或“继续”：按本文件最新下一任务接续研发，不重做已完成工作。新会话首次先读取本文件、实际分支 HEAD；若 HEAD 已推进，只检查本基线之后的差异及相关 Actions 结果，不能回退到旧版本。
- 目标：2017 Audi A6L C7 PA，MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32，纯软件无线 CarPlay，保留有线，蓝牙引导 + Wi-Fi，5GHz 优先、2.4GHz 回退，不绕过 MFi。先打通首连，再优化；不与仪表地图项目混合。
- MIB Toolbox 3.8、SSH、原厂蓝牙及热点已有。5GHz 真实硬件能力未知。最终 SD 安装、备份、校验、回滚需真实证据；目前禁止安装。
- 项目事实以 GitHub 文件、提交、Actions 原始日志、明确来源的实车证据为准，不以聊天摘要或主观百分比为验收依据。

## 2. 当前状态和下一任务（R105.156 → 后续）

**当前 R105.158：按第13节固定攻关总表逐项推进；下一项第7、8项，追踪原厂解析器与Provider调用参数来源。** 已通过 GitHub repository Actions runs GET 查到 R105.156 run 45998478475，两个 job 均 SUCCESS；原始日志分别明确打印 R105156_EVIDENCE_ONLY_NOT_CONSUMER_VERIFIED。reader-targets 解码两个 callee 但没有证明配置 consumer；boot-path 扫描201个文本文件，找到启动脚本路径候选，未证明实车挂载或生效。保留首连配置第03步90%，不重跑此测试。

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
- 独立工作每轮尽量充分利用最多20个并行云端job的预算；能独立执行的有效研发和测试一次性安排，不再默认15路；不凑数量。提交前检查现有运行及排队的任务、自动触发的回归和矩阵展开数，将新增并行任务限制在剩余预算内；单矩阵max-parallel不超过20且须为其他工作流预留容量。存在依赖的测试按先后执行，不为并行而破坏依赖；不重复已通过的相同输入测试。实际并发由GitHub调度和外部任务决定，不保证20路同时运行。已通过的同输入 CI 直接复用，除非相关输入变更、证据冲突或产物确实丢失。优先复用成熟 GitHub 源码及已有原厂文件；确实缺失才一次性列出路径/镜像/用途。
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

## 10. 第01项集中攻关：实车只读收口（2026-11-27）

用户要求先攻关第01项。严格计分仍3/5（60%）：原厂单字段候选、副本修改恢复、QNX探测工具已经通过；真实有效路径/reader和配置生效两项未验收。先前90%为旧研发估算，不再作为验收比例。

本轮重新审查 R105.155 probe.c/probe.sh、R105.156 reader.py 及实际 artifact 列表。无需重编或重复静态扫描。现成可用只读探测包：[R105155-MU1005-READ-ONLY-PROBE](https://github.com/roothongkong-ctrl/AU57x-R22-QNX65-Build/actions/runs/45995760126/artifacts/15196840612)（本轮核实未过期，5826 bytes）。包含 QNX ARM helper、probe.sh、README_CN.md、SHA256SUMS；只读检测，不是安装器。用户可将解压后的文件一起放在车机可读目录，在车辆停稳、原厂有线会话结束时，经已开启 SSH 进入该目录运行 `sh ./probe.sh`，完整返回输出（不包含配置原文）。不执行写入、重启或remount；若 helper 不可执行/路径不可读则停止，不能猜设备路径。实际使用前核对包内 SHA256SUMS。没有实车 SSH 权限的云端不能代替这次采集。

验收：SAME_FILE/CONFIG_HASH/MOUNT_READONLY 只证明身份/候选及挂载信息；CONFIG_FD 可提供进程打开该文件的证据，不等于读取或应用 enableIap；未看到 FD 也不证明未读取。须结合真实原厂reader/服务状态才能记第四点，真实配置生效才记第五点。若需受控配置实验，先检查可恢复性、原厂流程及安全门禁，不能强行改只读 EFS。当前无真实实车结果，60%不变。本轮只整理并验证已有可用探测产物，不宣称新增功能。第01项若实车暂不可测，可按此前规则并行推进第02项，避免停在无新输入的云端任务。

## 11. R105.157 同平台成功方案的原厂启动路线（2026-11-27）

已直接核实 [MU1367 v1.6.1](https://github.com/tomasz-twardzicki/MHI2-Carplay-Maps/blob/fda1104067d1ed653a194c50c222ba3fecde375f/MU1367/wireless-native/README.md) 与其[原厂验证报告](https://github.com/tomasz-twardzicki/MHI2-Carplay-Maps/blob/fda1104067d1ed653a194c50c222ba3fecde375f/MU1367/wireless-native/FIRMWARE-VERIFICATION.json)：维护者报告11月19日test2实车成功。其来源为 omonob 方案；[启动包装脚本](https://github.com/omonob/VW-MHI2-Wireless-Carplay-Maps/blob/aec37f5b1c87a0d961d16fe4328ac3638d0a833e/standalone/SD_ROOT/carplay/payload/wireless/common/ota/btstack_wrap.sh) 保留原厂btstack执行，按开关加载专用接入层，接收器启动环境独立。不是单纯enableIap开关。该方案部分接收器涉及应用许可变更，不纳入本工程；保留原厂MFi及应用授权。不直接移植二进制或外部固件地址。
- 原厂btstack SHA：MU1005 51ecbd44...，MU1367 dd80ce8f...、MU1102 896c9d58...、MU1440 d2b55046...，均不一致；MU1367的13个指令签名仅证明其自己的profile。connectivity.json同为8699字节但哈希不同，不是同一配置。
- 用户上传001(6).zip本轮实际检查CRC，20份snapshot，实车pidin中已有connectivity_launcher、原厂btstack及Bluetooth；/mnt/system为flash挂载。OEM复制0文件，不能把路径列表当文件内容，也不能证明reader或开关生效。不发布原始日志/个人标识。
- 复用本仓库现有 tools/mu1005_efs_launcher_stage.py 的原厂 launcher.btstack.exec 单字段离线适配及 tools/r94_launcher_audit.py 的固定原厂launcher SHA。R105.157新增 lab/mu1005_donor_launcher/trace.py，针对不同的原厂程序 connectivity_launcher，定位 setExecutable/JSONReader 直接调用及配置路径引用，拒绝不匹配的ELF；不再重复Bluetooth enableIap静态扫描。代码提交 3fb80582，CI配置提交 e493fada6；工作流 .github/workflows/r105157-donor-launcher.yml 复用已校验NAS artifact，仅提取app70，不重编QNX，不安装或修改OEM。当前新CI最终结果未取得。
- [joeyQuery MU0678 roadmap](https://github.com/joeyQuery/MHI2-wirelessCarPlay/blob/3db007ae20226adc3e1aab4ca85c6626c09178f8/docs/roadmap.md) 明确enableIap是Bluetooth topology/reconnect policy而非btstack启动/端点创建开关；仅作另一版本证据，不直接推定MU1005同义。
- 本轮第01项保持3/5（60%），第一阶段可沿原厂launcher适配路线推进；下一任务检查R105.157真实CI结果，按实际调用证据决定能否复用既有启动适配，原厂接入/实车读取生效未闭环。任何静态候选/诊断通过均不计实车生效。NO INSTALL。

用户最新确认：每轮尽量一次性安排所有可独立进行的有效攻关和云端测试，总计划并行预算不超过20路；汇报通过/失败/运行中的实际数量。

## 12. R105.158 原厂启动管理器下一调用链
R105.157 run 46004940954 / job 140525636386 SUCCESS。实际日志确认原厂 connectivity_launcher 在 0x10e310 调用 OSAL ProcessTNG::setExecutable（PLT 0x108278）；JSONReader 无直接符号候选；connectivity.json 字符串 VA 0x136402，旧算法无直接引用；两个绝对路径字符串未找到。不是实际reader或配置生效。第01项仍3/5。R105.158 4路独立：setExecutable 参数寄存器窗口、EHABI候选函数与调用者、确切文件名指针及PC相对访问、实际文件/配置解析导入与调用点。复用已校验固件，无NAS二次请求，不重编QNX，不重跑R105.157，不执行原厂程序。四路静态观察结果及空结果均应如实记录，只有证据闭环才更新验收。提交时既有近期工作流已完成；预计新增4路专项，另通用SI/CPRX可能自动触发4路，计划共8路，不凑20。下一轮读取实际日志后只追有意义的新边；若无有效线索，将第01剩余两项待实车验收并转第02，NO INSTALL。

## 13. 固定细化攻关总表
用户要求将全部任务固定列出逐条解决。已建立 [WIRELESS_FIRST_LINK_TASK_REGISTER_CN.md](WIRELESS_FIRST_LINK_TASK_REGISTER_CN.md)，10大项、66细项，保留既有正式进度比例，不把任务数量当功能完成比例。R105.158专项46005454333及回归46005454267均4/4成功；解析器调用点0x11603c/0x1179ac，Provider构造0x1280f0，fopen64 0x110454，setExecutable参数r0=r4/r1=[sp+0x30]，实际文件关联未证明；无前置unwind入口和文件名绝对指针命中。第01项60%不变。下一任务第7、8项，定向解析器/Provider参数及配置文件关联，不重跑旧检查。每轮只更新总表中实际状态变化，完整历史见Git。此提交仅整理验收和计划，不是新增功能。

## 14. 用户确认连续编号（2026-11-27）
66项攻关总表已经统一改为整数1–66，原任务内容、顺序、验收状态及10大项统计不变。今后以总表连续编号报告及推进，不再用1.7等小数编号。当前第7、8项；后续第9、10、11项。最小安全恢复第63、64项须在受控实车修改前满足。仅调整编号，不增加功能进度。

## 15. R105.159 只攻关第7项
用户明确先完成第7项再讨论第8项。复用现有原厂参数转发wrapper，不重复静态扫描，新增12项独立host实际exec、1项原厂精确配置参数保留、1项QNX shell+ARM目标实际执行测试；另预留4路自动集成回归，总预算18。报告见 [R105159_STEP7_ARGV_CN.md](R105159_STEP7_ARGV_CN.md)。目前未取得CI结果，第7项仍部分完成，第01主项60%不变。真实原厂supervisor与配置reader不以夹具替代，NO INSTALL。下一轮只核实第7项的实际结果和修复失败，不提前进入第8项。

## 16. R105.160 第7项并行收口
R105.159 run 46007850122：12/12主机参数转发及精确原厂配置job 140535457592均通过；通用回归46007850078 4/4通过，总17/18。QNX job 140535457283仍在Build verified toolchain，不重复启动相同编译。原始配置日志确认btstack节点只有customer、exec、maxUnhealthyReports、onFailure、preCondition，无显式参数字段；副本仅改exec，原始SHA 56c14940...，新SHA 2cf6a5339deeafa1f0636475f23eb0ace83c891f0b41e2e5b127939d876bcc82。不能据此断言原厂实际参数来源。新增R105.160两路独立定向原厂参数生产/消费检查：launcher的非setExecutable ProcessTNG参数及启动API；原厂btstack入口/参数解析导入。复用精确app70，不重新编译QNX，不重跑先前扫描，不进入第8项。提交前原QNX一路运行，另MIB审计3个job已完成；新增2路+4路自动集成=最多7路计划。第7项保持部分完成，仍待真实QNX测试及参数来源证据，NO INSTALL。

## 17. R105.161 第7项针对失败修复
R105.159 QNX job140535457283最终FAILURE，原始日志实际 STEP7_TARGET factory pid=4 argc=2、exit0、STEP7_QNX_ARGV_PASS factory pid=4，证明确实在QNX guest内完成第一例数字参数转发且同PID；第二例spawnv shell ENOENT，supervisor exit1，不能认定其余4例通过。artifact15201469833。其余R105.159/160累计23/24 jobs成功（其中4路重复自动回归，并非新功能）。R105.160 run46008695322原始日志：ProcessTNG::start 4个调用点0x10ffe0/0x11ca28/0x11cb20/0x11f470；btstack main 0x118868、strtol 0x118a40，argc/argv来源仍为候选，不是实际参数生产证明。R105.161新QNX专用workflow只运行剩余4例，fork/execve替换实验supervisor spawnv，逐例独立结果、stat/errno诊断，既有wrapper/目标不改，保留原成功证据。不假定已找到ENOENT根因。另一路使用原固件直接依赖唯一OSAL provider的ProcessTNG::start符号追踪实际spawn入口，禁止重复旧全量扫描。当前计划2路专项+4路自动回归（提交前无活动run），第7项仍部分完成，NO INSTALL；仅验证实际结果，后续8未启动。

## 18. R105.162 第7项原厂provider查找修复
R105.161回归46009278216 4/4成功；QNX修复run46009278959/job140540320980仍在构建，不重复运行。原厂provider run46009278340/job140540317849失败，实际错误expected unique original ProcessTNG provider, found 0。旧程序仅搜app70中名称带osal且有非零大小的直接依赖，范围不充分。此次增加精确stage2/EFS，解析真实ELF SONAME及定义符号（含零大小），按SHA去重，保留多候选而非猜唯一，不执行原厂程序。若仍零命中，记录原厂参数来源尚需运行时证据，不再扩大同类扫描；原样参数转发软件验收与原厂启动接入后续验收分开。新增1路专项，另预留4路回归，加原QNX一路共6路计划。第7项保持部分完成，NO INSTALL，不进入第8项。

## 19. R105.163 编译提速及第7项收口原则
用户同意优先原样传递原厂启动参数，不需要自行生成参数；保留原厂真实启动接入证据在后续任务，不能把转发夹具当实车启动。R105.162 run46009613514/job140541479015扫描656库，launcher DT_NEEDED含libosal.so，只有stage2/eso/lib/libosal.so候选，未找到定义符号且无解析错误，停止重复扫描。回归46009613050 4/4通过。R105.161 QNX修复run46009278959仍须核实。新增[R105163_QNX_IMAGE_CN.md](R105163_QNX_IMAGE_CN.md)所述完整编译镜像缓存、独立新runner强制命中验证，未来仅改程序则不重建编译器。当前新机制待验，不虚报提速；不改正在运行的QNX任务，不进入第8项。新增2个顺序job，预留4路自动回归，现有任务1路，计划不超过20。NO INSTALL。

## 20. R105.164 第7项剩余场景独立QNX启动
R105.161 job140540320980最终失败，实际STEP7_TARGET empty pid4 argc4 exit0及STEP7_QNX_ARGV_PASS empty pid4；之后三个场景均fixture stat ENOENT。不能确定哪个文件消失或根因，不能归为生产wrapper缺陷。保留R105.159 factory和R105.161 empty已过的两例，不重复。新增none、missing、exit三路独立guest，每个只运行一个case，分别检查shell/wrapper/target存在，真实目标和supervisor同PID及预期退出状态必须成立。统一先确认既有缓存run46010604447完整通过，再require-cache=true直接恢复，缓存未通过则失败且绝不自行重建。原缓存任务实际日志仍GCC正常构建，未发现致命错误；不改其缓存key或正在运行的任务。新增3路+4路自动回归，当前活动1路，另预留缓存验证1路，共不超过20。仅实验环境隔离，未证明长时间多次调用的QNX环境文件消失根因。第7项部分完成，不进入8，NO INSTALL。

## 21. R105.164验收完成与第8项交接
R105.163完整缓存验证run46010604447 2/2成功。job140544815944编译与保存成功；job140549422382实际恢复291152342字节缓存、image.tar.gz SHA OK、QNX_COMPLETE_IMAGE_CACHE_HIT、QNX_COMPILER_SMOKE_PASS、R105163_FRESH_RUNNER_REUSE_PASS；从恢复日志13:59后构建到14:00:24缓存恢复至14:00:37编译验证约13秒（不含排队和其他测试）。R105.164 run46011767233 3/3成功，三路实际cache hit；missing job140548805528，none job140548805847，exit job140548806227，目标和supervisor同PID验证，退出码0/0/23，均独立QNX guest。R105.159 factory和R105.161 empty的真实成功子例保留，合计5/5场景。通用回归46011767074 4/4成功。用户已同意原样转发，停止参数生成机制重复搜索。第7项限定软件验收完成；原厂真实启动加载留10、17，重复调用模拟文件消失原因未证，不认定真实车机稳定启动。固定总表28完成4部分34待完成，正式主项评分不变。下一第8项原厂配置读取调用链，使用既有解析器入口和实际参数证据，不重跑旧扫描。后续QNX复用已验收action，warm后再并发，缓存失效只构建一次；不宣称永久。此提交仅证据归档，不开始第8项，不启动新专项。NO INSTALL。

## 22. R105.165 第8项定向攻关
第7项限定软件验收完成，现按用户要求仅攻关第8项。现有R105.158原始调用点JSONParser::parseFileToArray 0x11603c/0x1179ac、Provider构造0x1280f0、fopen64 0x110454，已知但实际connectivity.json未关联。新lab/mu1005_step8_reader/reader.py对四个精确调用点并行：验证原厂SHA及调用目标，回溯局部ARM寄存器常量和PC相对字符串、候选函数边界、附近实际调用，保留原始指令窗口供复核。复用已有固件与分析器，不重跑全局扫描，不编译QNX，不修改OEM。C++对象/堆栈参数与间接路径不能靠线性追踪宣称证明；无关联必须如实记录，后续选择最短实际边或受控只读运行验证，禁止无休止扫描。4路专项+4路自动回归，提交前无活动任务，预算8。第8项仍部分完成；第9项未开始。NO INSTALL。总表28完成4部分34待完成，正式主项不变。

## 23. R105.166 第8项四路方案
R105.165 run46013467170 4/4及回归46013466993 4/4成功；原始日志 parser-a/b r3=0、parser-b UnicodeString8::utf8 0x117958、Provider附近JSONReader字样及snprintf 0x1280a8、fopen后fclose；未得到目标文件实参。用户同意四方向：锁定omonob原始源码接入点审查（不执行、不复制授权绕过）、已知UnicodeString与Provider对象/寄存器来源、精确原厂配置语义对比、QNX原厂libutil真实dlopen/dlsym预检和只读fixture哈希一致。复用已验证镜像，require-cache=true；未知C++对象布局/参数契约不得猜测调用，故本轮原厂parser只预检导出而非原厂parse执行，实际解析调用和文件关联仍待证。现有host对比复用原配置验证作为新预检基线，不以重复校验增加进度。4专项+4自动回归计划，第8项部分完成，第9项不启动。NO INSTALL。

## 24. R105.167 第8项对象上游与真实解析库ABI
R105.166 run46014286871：donor job140557318887、config-contract job140557318270、reader-object job140557319018成功。donor仅证明原方案有connectivity.json事务与btstack wrapper，不证明MU1005运行；原始配置对比只改exec，原厂parser未执行。原始指令parser-b 0x11799c r1=sl、0x1179a0 r2=sp+0x110、0x1179a4 r3=0、0x1179a8 r0=fp，sl来自UnicodeString utf8返回对象分支；0x117954 r1=[sp+0xe0]。Provider入口0x128058，构造0x1280f0 r1=sb来自[r8+8]、r2=r8来自[sl+8]，初始r8=r1、sl=r2。新专项追踪0x128058直接调用者、sp+dc/e0写入及原厂libutil.so真实定义的构造/析构/parseFileToArray ABI。只保留静态证据，不猜对象尺寸或执行未知ABI，不重复R105.165/166。现有native-preflight job140557318911仍运行，已进入缓存后的QEMU环境步骤；新增2路专项加4路自动回归及现有1路，预算7。第8项未完成，下一项未开始。若无上游关联，停止同类扫描，采用原厂只读运行证据。NO INSTALL。

## 25. R105.168 第8项原厂库预检路径修复
R105.166 native-preflight job140557318911实际失败：SDK_CONFIRMED_PIC_LINK_OK，BT_PINNED_SDK_RUNTIME_STAGED，REAL_LOGGING_PROVIDER_STAGED均成功；ARM链接器cannot open output file /home/runner/work/.../qnx-runtime-evidence/reader/step8-parser: No such file or directory。宿主绝对路径错误传入容器，实际映射/src。只改编译输出为/src/qnx-runtime-evidence/reader/step8-parser，宿主检查产物非空，保留原QNX预检，不改变原厂文件/ABI，不重复其他三路。R105.167 run46014738003两路成功，producer实际发现Provider入口直接调用者0x122e88、0x123548；UnicodeString栈槽只得到读取，未找到写入；native-abi输出libraries=[]，尚未找到带非零大小的定义函数，不能说解析库ABI确认。该项仍未完成。新增1专项+4自动回归，require-cache=true，无完整编译器重建；不启动第9项。NO INSTALL。

## 26. R105.168 两路原厂库预检成功与用户同意收口方案
实际run46015546454/job140561611027及run46015546355/job140561611649均SUCCESS；原始日志两路均QNX_COMPLETE_IMAGE_CACHE_HIT、R105168_CONTAINER_OUTPUT_PATH_OK、STEP8_NATIVE_EXPORT_OK 0/1/2/3、STEP8_FIXTURE_READ size=8699 fnv=a82094ed、两个进程exit0、R105166_QNX_NATIVE_EXPORT_PREFLIGHT_PASS。故原厂libutil真实加载及4个导出接口、独立只读配置副本读取通过；未执行原厂parseFileToArray，未证明原厂解析器与connectivity.json实际调用关联。另重复触发旧workflow是共享脚本路径匹配所致，不再重复预检。用户同意保留原厂读取机制、复用成熟接入方案、验证配置兼容性，不重写解析器，不继续无休止静态逆向；第8项只需取得调用链有效证据，第9项验证实际路径、第11项实车生效。第8项仍部分完成；下一步以原厂读取的只读证据为主，不猜C++ ABI，不提前开始第9项。总表28完成4部分34待完成。NO INSTALL。

## 27. R105.169 按用户同意的omonob模式验证原厂真实调用
保留原厂配置读取逻辑与解析器，不重写、不复制donor授权修改；既有配置对比/原厂库预检不重跑。新建独立QNX guest，按原SHA选取connectivity_launcher，复用已验证编译环境及真实库依赖；只观察原厂parseFileToArray调用的文件名参数。ARM尾转发保留原始r0-r3、栈和返回契约，实际调用真实libutil，不猜C++对象布局；未找到真实函数则退出，绝不伪造解析成功。仅实验环境，不安装、不改原厂文件；真实目标文件名出现才认定调用关联观察到，仍不代表实车路径或配置生效。若原厂启动被服务/硬件依赖阻塞，记录实际阻塞，不重复逆向全固件。1专项+4自动回归，预算5，第8项未完成，第9项不启动。NO INSTALL。

## 28. R105.170 QNX旧SDK字符串兼容修复
R105.169 run46016668674/job140565432928最终失败，实际缓存命中、原厂launcher及依赖准备已过，编译错误strnlen implicit declaration，-Werror导致未进入guest。仅替换为最多512字节的有界长度循环，不改变读取观察、尾转发、原厂逻辑或验收条件；新增1专项+4自动回归，提交前无活动任务。用户要求对话只汇报通俗结果，完整日志、源码与进度保存在GitHub。未完成8，不进入9，NO INSTALL。

## 29. R105.171 原厂启动参数修复、停止无关回归
R105.170 run46017228755/job140567350546实际编译成功、原厂launcher字节校验成功、pid4启动，随后CoreApiLocator.cxx:13 no AgentStarter instance、Servmngt.cxx:40 the bundle loader commandline is invalid，libcomm ProxyTracked空指针SIGSEGV。之前仅传argv[0]不够；具体参数与服务是否齐全未证，不归咎原厂解析器。新专项先从已提取原厂启动文本找明确完整-sm-name/id/time/restart实参，并记录真实libservmngt命令行标记；仅唯一明确原厂命令才生成argv并复用原只读观察，不能猜参数或伪造服务。若无明确命令，保留诊断并停止无效重启。用户要求只测试受影响内容，自动集成workflow对独立step8实验、两交接文档及该workflow自身变更跳过，其他变更仍原样触发，可手动运行。此次只计划1路专项，不重复4路无关回归。第8项仍部分完成，第9项未开始。NO INSTALL。

## 30. 第8项限定方案收口与66项复用审查（用户同意）
R105.171 run46018322716/job140571111196失败：原始dsistartup.json SHA c0fe47842c83ae88a5de608dbc65bcfc7b47df513462775566fe2467c479f2f4 包含connectivity_launcher init/run/set_app_runmode（如925184），但没有唯一完整-sm启动实参，STEP8_STOCK_ARGV_NOT_UNIQUE_NO_GUESSED_LAUNCH，未再次启动原厂程序。用户同意保留原厂解析器、按omonob配置接入方式复用，结束第8项软件方案攻关；这是调整后的限定范围完成，不是原厂parse调用实测成功。原厂实际读取与生效证据移交第9/11项，禁止将旧失败改写为通过。下一第9项，尚未启动。66项审查：第10项优先复用成熟启动包装器（已有离线适配，实际接入留17）；第29、35、46、51项优先沿原厂认证、热点、服务发现、接收器接口复用，必须完成MU1005接线证据，不能仅因原车有线可用或其他固件成功直接标完成；第15/16/23/24仍需适配。第18/19/25/26/30/31/36/37/38/47/48/52/53/54与55-59实车联合取证，避免重复上车与重复模拟，不得省略真实验证。第42/43/65/66后置，63/64必要恢复前置。其他编号不变，不虚报完成。当前29限定完成、3部分、34待完成；正式主项评分不变。此次仅登记，不启动云端新测试。NO INSTALL。

## 31. R105.172 五项原厂复用统一执行
用户同意统一处理第10/29/35/46/51，具体复用边界见[R105172_STOCK_REUSE_CN.md](R105172_STOCK_REUSE_CN.md)。第10按现有原厂配置离线及QNX转发证据限定软件适配完成，第17实际启动不变。29/35/51已有代码适配但无真实owner，标部分；46新增定位专项，不能假定daemon正常连接。新统一只读观察工具只记录固定FD/路径候选，不推断owner；与第9及首连联合验证，勿重复旧测试。云端2路，提交前无活动任务，新增路径排除无关自动回归。状态30限定完成、6部分、30待完成；正式主项评分不变。第9实际路径仍待，未跳过；实车63/64前置。NO INSTALL。

## 32. 剩余36项路线按用户要求重排
已完成30项从日常清单移除，保留原固定66项作为历史和验收映射。新的工作顺序为1—36，详见固定总表末尾新路线。新1=原9；新2=原16，新3=原23；新4/5=原63/64，任何实车写入前必要恢复；其后按原厂加载、蓝牙、iAP2、MFi、AP、接收器/网络、Bonjour、同次真实首连、双频、正式SD交付依赖顺序执行。先完成当前项才开始下一项；若需真实硬件，明确阻塞，不伪造通过、不以新编号覆盖旧证据。既有R105.172专项保留，本次仅文档，不新启动测试。当前30限定完成、6部分、30待；尚未开始新1。

## 33. R105.173 新第1项（原9）开始；旧专项收尾
R105.172 run46019620969两路实际成功：job140575558757 NEW_COMBINED_OBSERVER_FILTER_TEST_PASS_NOT_OEM；job140575559252 STOCK_DAEMON_BINARY_LOCATED_NOT_STARTED，原厂app70 armle/usr/sbin/mdnsd SHA843581c874bf075854cb584e827117f609e8dc697ff22509d296444a413273f2，libdns_sd.so.1 SHA9636f6a0b650ce5ba0d0c1550e6f64e0d67dc172f03b98a48c2bd72b852bec35，双方均含/var/run/mdnsd，原厂dio_manager.json 22/25-28行及etc/scripts/mdnsd.sh 3行有关；仅定位，不宣称连接，非新第23项开发。新第1项原厂EFS配置SHA已知，真实车辆/mnt/system已挂载但无配置文件实体和实际reader证据，历史资料未找到进一步实车确认。复用R105.155 run45995760126已通过的ARM helper artifact15196840612（仍可用），只补connectivity_launcher FD查询及helper缺失直接停止，生成独立只读包，1路云端仅校验原产物/新脚本并打包，不重跑QEMU或旧回归。原始EFS候选/mnt/system/etc/eso/production/connectivity.json，/etc/eso/production/connectivity.json是否别名待实际同文件证明；禁止假定。新1仍部分，待实车输出或用户明确同意限定收口，未开始新2。统计30限定完成、7部分、29待；正式评分不变。NO INSTALL。

## 34. R105.173 新第1项只读检测包验收
实际run46020545537/job140578749861 SUCCESS；原R105.155产物三文件SHA校验通过，新包三文件SHA校验通过，ROUTE1_REUSED_EXISTING_QNX_PROBE_READY_NOT_CAR_VALIDATION。新artifact15207563136（5940字节，未过期）。无活动云端任务；没有新的实车证据。原厂EFS路径已知，但当前车辆实际文件身份/读取仍未确认。已完成全部可执行离线检测准备，不再重复模拟；若用户暂不上车，须明确同意将实车路径核实并入新4/5的只读预检，方可收口新1限定软件范围；任何写入前必须确认路径和恢复。尚不进入新2。30限定完成、7部分、29待。NO INSTALL。

## 35. R105.174 新1收口、新2单项推进
用户在明确建议将实际路径确认并入新4后回复1，按既定继续口令执行：新1限定离线准备完成，实际路径与读取仍需新4/5及新7验证，任何写入前不得省略路径核实。新2原16开始。复用R38/R39、R98、既有OSAPI notify/owner及已验证回调布局，不重复旧测试。发现native_binding.c原厂回调进入后直接读取event头并将原始指针传入mu38_dispatch，绕过已存在checked read_live边界；本次改为一次16字节有界复制并重新检查owner，后续全用本地快照；读取失败时若仍为认证owner且链路活跃，沿现有IN_DIRTY/au_ipc_socket_lost逻辑断开，不伪造消息。无修改原厂程序、无新增签名/授权、无安装。仅新增1路，测试新快照失败、地址溢出、复制隔离并ARM编译实际改动，不重跑其他4路集成；该两个文件专属workflow覆盖后续单独修改。实际原厂owner及回调生命周期证据仍缺，不能称已接通。新3未开始。31限定完成、6部分、29待；36路线不再重编号。NO INSTALL。

## 36. R105.174 新2专项结果与真实接入边界
run46021591240/job140582324465 SUCCESS，实际日志 ROUTE2_NEW_CALLBACK_SNAPSHOT_TEST_PASS_FIXTURE_ONLY、QNX_COMPLETE_IMAGE_CACHE_HIT、ROUTE2_CHANGED_CALLBACK_ARM_COMPILE_PASS_NOT_LIVE_BINDING。源代码变更还触发统一开发workflow run46021591144，9路全SUCCESS（native-library、6类parallel-host、upstream-graft、collect-review）；这是已有通用CI自动触发，不误称只实际运行1路。已复核native_event/native_binding、R105.014原厂owner边界、BT_NOTIFY_BINDING、R92预加载：回调16字节布局及软件适配已有，真实原厂iAP2 owner、锁/生命周期及服务注册仍未取得，现有预加载探针只能证明进入进程及线程创建，不能代替owner。没有新车机数据；不重复已通过测试，不新增模拟owner。建议用户确认后将真实owner/回调接入联验并入新6/8/9；新2按软件范围收口，未获确认前仍部分，未开始新3。此次仅记录结果。NO INSTALL。

## 37. R105.175 新2限定收口、新3专项
用户在建议新2软件范围收口后回复1，按继续口令执行。新2限定完成，实际原厂owner、回调锁与生命周期接入证据保留在新6/8/9，不认为已连通。新3原23复用R105.011/013/018/019/020、R105.115/132等：已知mm-ipod mount/io_msg薄转发、device+0x38 context及+0x24回调槽、外层Transport +0x14/28/2c不同对象，不重跑USB加载、IPC拒绝、全固件扫描。新增1路只从已保留固件取精确mm-ipod和driver SHA，追原厂driver ipod_module+0x20 relocation及候选接口+0x24槽，结合mm-ipod driver_init/iface_find已有函数区间核实对象关系，记录候选函数及opcode立即数。不能把单一槽位或立即数认定为实际分派；不能把原厂USB transport当BT。未改原厂代码、无安装、不启动QEMU。待实际结果，新3未完成，不启动新4。32限定完成、5部分、29待；正式评分不变。NO INSTALL。

## 38. R105.176 新3专项工具导入冲突修复
R105.175 run46023609556/job140589224873实际失败：本地inspect.py遮蔽Python标准库inspect，pyelftools经pprint/dataclasses循环导入probe，未执行原厂descriptor分析。将工具改名driver_dispatch.py并删除原inspect.py，更新同一专项workflow调用，在提取前增加标准库inspect.signature检查，原分析逻辑不变。只触发同一专项1路，不重复其他测试。新3仍部分，未开始新4，NO INSTALL。

## 39. R105.177 新3原厂文件身份修复
R105.176 run46023875819/job140590138224实际失败，标准库导入检查与固件提取成功；分析脚本 ValueError: missing exact mm-ipod。历史交接/静态审计的 mm-ipod SHA 字符串为65位，永远不能匹配SHA256；不猜删哪一位。复用已验收R105.127原厂路径 app70/filesystems/MMX2/app/70/default/app.img/armle/usr/sbin/mm-ipod 和已校验整包SHA 6946216d...；记录实际文件SHA、尺寸、ARM ELF及历史函数锚点，尺寸40297及至少两个已知符号锚点匹配后才继续旧descriptor专项。driver仍使用原64位SHA固定值；失败亦上传脱敏身份JSON。保留历史错误原文以便追溯，不能把候选哈希当历史精确身份；不修改原厂文件，不重复已过功能测试，新增同一专项1路。新3未完成，未启动新4，NO INSTALL。

## 40. R105.177 实际验收及 R105.178 新3续攻
R105.177 run46024744384/job140593107045 SUCCESS，artifact15211206944（11126字节）实际下载并复核两个JSON。mm-ipod真实SHA按8位分组为 4b87cd2d 41ddcee2 f38800fe afe8b4b3 40e4f23e 1d86c7e9 7af30352 2aa15d09（64位，历史多一个8；不猜测），40297字节，三个函数锚点全部匹配；driver SHA原固定值匹配，739925字节。静态证据：iface_find 0x103498 读取原厂descriptor+0x20，0x103504返回接口；device_drvr_init 0x103698 将返回接口写入device+0x38，0x1036a0反写owner至接口+0x14。原厂iap2 driver descriptor 0x27e7c ABI 0x200，+0x20 R_ARM_RELATIVE 指向iap2_drvr 0xa9bb0；该表+0x24指向iap2_msg 0xf4c8，其他+0x28/2c/3c/40/44分别对应notify/unblock/open/read/lseek，闭合旧R105.019静态回调映射。iap2_msg 0xf500读取消息+6的16位字段，0xf504/508比较0x9999，匹配后0xf528调用MsgReply；0xf79c比较0x3000，0xf7a0不等跳错误分支，匹配经0xf7a4跳0x10680。原产物4096字节窗口只解到0x104a8，未覆盖0x10680，不能猜原始消息发送后端。表+0x20为transport_recv_pkt 0x2121c，尚不等于无线producer。R105.178仅从已校验整包提取app70（不再提stage2），独立检查此前未覆盖的0x104ac至iap2_msg符号末尾及精确transport_recv_pkt函数；不重跑旧descriptor、已通过测试或QEMU。1路专项，提交前活动任务0。原始产物与日志保留；真实无线producer、运行时接入仍缺，新3仍部分，未启动新4，NO INSTALL。

## 41. R105.179 新3反汇编窗口修复
R105.178 run46025185470/job140594643481 FAILURE；原厂app70提取及driver SHA检查通过，分析脚本 ValueError: raw branch not decoded，旧线性Capstone反汇编未到已知0x10680目标（具体阻断字节尚未保留，不能认定原厂逻辑错误）。本轮按4字节独立解码已限定ARM范围，保留无法解码间隙、ELF $a/$d/$t映射，不将已标记数据/Thumb当ARM；单独从已知0x10680分支目标及transport_recv_pkt入口取窗口，先写JSON再做入口验收，失败保留证据。无重复已过descriptor或功能测试，无QEMU，1路专项。第3项仍部分；不进入第4项，NO INSTALL。

## 42. R105.179 实际验收及 R105.180 新3接口契约攻关
R105.179 run46025407820/job140595421320 SUCCESS，artifact15210309557（3473字节，未过期）。原始日志0x10680 mov r0,r5；0x10684 add r1,r6,#0x14；0x10688 ldr r2,[r6,#0xc]；0x1068c实际调用link_send_raw_message 0x113a8，检查返回并在成功路径0x106ac MsgReply。接收入口transport_recv_pkt实际调用link_get_transport_data 0x12208、packet_parse_hdr及0x21468 link_handle_iap2pkt 0x12e00。至此原厂resmgr消息发送及内部接收静态链闭合，不等于无线producer。复核R105.011/012、R98/R99/R103、cp_step2_transport.h：现有RFCOMM桥非原厂插件ABI；优先保留原厂iAP2，已有桥可复用，但不能猜原厂函数参数。R105.180仅检查尚未闭合的真实参数与返回值：已知driver send/receive调用点及USB donor对应函数入口、已发现raw send和get transport data函数体，复用原始SHA固定的app70；不再测试旧消息回调或重跑整套回归。仅1路专项，提交前活动任务0；不会把USB donor当无线插件，真实owner/接线未验证，仍新3，未启动新4，NO INSTALL。

## 43. R105.180实际结果及R105.181字节数适配
R105.180 run46025811674/job140596793519 SUCCESS，artifact15211048329（5337字节）。原始日志确认driver 0x211e8/211ec/211f0将owner、packet+0x18指针、+0x1c长度传给+0x28；0x211f8-21204仅在返回实际字节数等于请求长度时成功。receive 0x21124-21138经+0x2c，r3及栈第五参数置0；USB donor空接收在r3=0时返回0，完整第五参数语义仍未知。raw send 0x11474调用link_send_pkt；receive 0x126b4调用transport_receive。现有cp_step2_transport_write返回异步接受状态，不可直接当原厂字节数。R105.181新增仅实验的byte_contract.[ch]：复用旧bridge/stream，要求独立真实完成见证及无pending才返回实际长度，缺见证不发送，超时或仍pending拒绝并隔离，接收仅提供无等待字节数/空0/错误-1；不导出ipod_module或猜原厂完整ABI。真实finish provider及锁/生命周期尚未得到，外部owner必须保证独占worker、可完成回调及实际断开后释放存储。新增专项仅测试此适配并ARM编译，不重提固件、不重跑旧回归；当前仍新3，未进入新4，NO INSTALL。

## 44. R105.181验收及R105.182复用原厂发送完成链
R105.181 run46026036834/job140597562155 SUCCESS，artifact15211247798（978字节），实际日志 ROUTE3_BYTE_COUNT_COMPLETION_FIXTURE_PASS_NOT_OEM、QNX_COMPLETE_IMAGE_CACHE_HIT、ROUTE3_BYTE_CONTRACT_ARM_COMPILE_ONLY_NOT_LIVE。复核原有MU1005 native_event.c event6 -> inbound_r5.c ir5_event（inbound_event核对packet，completion=1）-> ir5_tick -> au_socket_tx_complete -> au_rf_send_complete；已有实际接入源码但未在真车观察。现有本机57342协议不传发送完成回执，不能用TCP写成功冒充SDK完成，也不能直接拿通用cp_step2_transport与原厂legacy桥混用。R105.182在实际复用的mu1005_rf_ingress增加同一代际完整归一化帧的本地完成序号及字节数；仅最后一段成功completion记录，拒绝跨代际、取消、失败、重复消费及跳号，旧接收/发送协议不变。新增专项验证MTU分段、失败/断开和ARM编译；真实SDK事件6仍由原有token/owner检查保护，尚缺原厂实际owner以及跨进程完成传递，不能把本地receipt称为真实手机接收或MFi。新3未完成，不启动新4；提交前无活动任务，计划新增1路及既有受生产源码变更触发的9+4路回归（最多14路），不手动重跑旧测试。NO INSTALL。

## 45. R105.182实测及R105.183复用已有ACK协议
R105.182 run46026319903/job140598516034 SUCCESS，artifact15212925067；日志确认 ROUTE3_EXISTING_RF_FULL_FRAME_RECEIPT_FIXTURE_PASS_NOT_NATIVE、QNX_COMPLETE_IMAGE_CACHE_HIT、ROUTE3_RF_RECEIPT_ARM_COMPILE_ONLY_NOT_LIVE。自动集成46026319865四路成功；46026319969六路host及upstream-graft成功，native-library仍在运行，collect待后续。进一步核对发现已有cp_step2_ipc_link.c/h支持type4零长度、同radio generation的SDK完成ACK，故不发明新协议、不新建独立socket。R105.183在既有mu1005_rf_ingress增加显式首连前opt-in，最后一段完成后才排入既有type4，默认关闭保持原有type1/2/3消费者兼容；队列满关闭，失败不发布成功回执；现有cp_step2_ipc_link接收端不修改。新增真实本机socketpair连接两端源码，验证默认无ACK、分段不提前确认、整帧确认及失败拒绝、ACK不计RX credit；SDK完成仍为夹具，非实车。尚未证明原厂运行时owner或将opt-in绑定到实际启动，不能安装。计划新增1专项+既有自动回归最多14路，另前一版本最多2路剩余，合计不超20。新3未完成，未启动新4。NO INSTALL。

## 46. 用户新增强制规则：每项先查GitHub、优先复用（2026-11-27）
今后新路线每一项及关键技术阻塞，必须先检索GitHub相关开源源码、最新分支/提交、issue、PR、实车成功记录以及本工程已有成果，再决定研发方案。优先顺序：同平台同固件直接复用（确认许可证及版本/ABI）；类似平台少量适配复用；复用原厂接口及已有模块；确实没有合适实现才自研缺失部分。每项记录上游仓库、固定版本/提交、可复用模块、适配差异、证据级别及未解决事项，避免重复检索已知未变化内容和重跑已通过测试。同平台实车记录与其他平台参考区别对待；不直接套用不同固件绝对地址，不绕过原厂MFi，不把静态/夹具测试称为实车成功。
第3项本轮公开GitHub检索候选：luka-dev/mib2q-carplay-rgi（现有主线与已复用协议/蓝牙graft）；congobg/MHI2Q_WiFi_CarPlay（QNX6.5 ARMv7、保留原厂receiver/iAP2 client，3638/5092异平台参考）；joeyQuery/MHI2-wirelessCarPlay（更接近MHI2，MU0678，明确蓝牙endpoint到resmgr仍未闭环，不能当成功方案）；chefranov/mhi2-au37x-carplay（HARMAN iAP2适配参考，未证明无线首连）；oguzkarcan/EntryNav2-CarPlay（BMW异平台，协议边界参考）。优先核对congobg的蓝牙接入与joeyQuery最新trace，对照本工程原厂数据路径，只复用合适且许可允许的实现。本轮仅登记检索结果和强制规则，不新增代码、测试或宣称兼容通过；R105.183原有专项保持不变。

## 47. R105.183验收、GitHub复用核查及R105.184
R105.183专项run46026551699/job140599310444成功，artifact15212303330；日志 ROUTE3_EXISTING_TYPE4_ACK_REAL_LOCAL_SOCKET_PASS_FIXTURE_ONLY、ROUTE3_ACK_ARM_COMPILE_ONLY_NOT_LIVE；自动46026551785四路、46026551688九路全部SUCCESS；上一版46026319969亦九路完成。提交前无活动任务。依据第46节先查GitHub：congobg/MHI2Q_WiFi_CarPlay 固定223b42f90bf7c15fbbbef23259da36caa9bd4994，公开树15项、docs/LICENSE为PolyForm Noncommercial 1.0.0，作者报告5092/3638异平台首连，蓝牙bootstrap与AirPlay内层原厂iAP2分离，未公开可直接移植C/Rust源码；不混入GPL工程。joeyQuery/MHI2-wirelessCarPlay 固定3db007ae20226adc3e1aab4ca85c6626c09178f8，TRACE016/017/018仅MU0678的原厂IapDevice/cbRf路径，未闭合路径producer，公开树无许可文件；仅参考事实，不复制文档/地址。MU1005 R105.014已验证对应IapDevice等字面量缺失，不重扫，不能将同类固件推论当本车证据。LuKa已复用的GPL协议及R103/原厂内层iAP2路径继续保留，不强制无线bootstrap走原厂/dev/ipod0。R105.184仅将已通过的byte_contract与既有cp_step2_ipc_link真实ACK解析连接，提供限定单worker、代际、确认计数、绝对deadline和最大step次数的完成见证接口；旧协议、原厂认证及生产代码不改。测试新连接的正常/分片ACK、超时、错误代际及断开；仅1路专项，ARM编译。真实进程owner及SDK事件未证明，不导出原厂插件。若本专项通过，建议用户同意新3按离线软件适配范围收口，实际producer/完整ABI/运行接线留新6/8/9/11/12，不再重复离线发送检查；未确认前仍部分，不启动新4。NO INSTALL。

## 48. R105.185 用户批准三项并行；新3限定收口、新4/5并行启动
用户明确同意3/4/5并行，最多20路含自动任务，独立工作并行，依赖验收按顺序；此规则覆盖旧单项串行规定，不改变编号、安全门禁。R105.184 run46027124565/job140601284673 SUCCESS，artifact15210994220，真实日志 ROUTE3_EXISTING_IPC_ACK_TO_BYTE_COUNT_PASS_FIXTURE_ONLY、ROUTE3_IPC_FINISH_ARM_COMPILE_ONLY_NOT_LIVE。新3按软件适配限定完成，实际producer/完整插件ABI、真实owner及接线移交新6/8/9/11/12，尚无真实无线首连。累计33限定完成、剩33。新4/5开始前检索GitHub：Mr-MIBonk/M.I.B._More-Incredible-Bash 3a2526a386f54ec7f49bbc32984ef7edb1106d33 GPL-2.0（已有用户MIB 3.8，备份/恢复作为外部独立工具复用，不混合许可证）；jilleb/mib2-toolbox af244e7cb8c912ab47f5c09cd7677370fe86b441 MIT（参考SD/恢复流程）；harman-f/MHI2-MHI2Q-MH2P-Firmware-Toolkit 64a44fe23dbaa08771440d69642d2874d7028295 GPL-3.0（复用现有固定提取工具）。新4新增原厂app70与EFS精确文件身份清单，区分归档路径与车辆实际路径，复用已通过R105.173只读探针，不再编译。新5复用已有67项事务及独立恢复控制器，新增独立备份字节验证工具及7项测试，只用于电脑端可信清单与导出备份核对，尚不具备车机恢复入口。3路独立新增云任务，不人为凑满20；实际目标路径、备份完备、恢复入口、实车持久性未证。NO INSTALL。

## 49. R105.185三路验收及R105.186三项并行
实际run46027509995三路SUCCESS：job140602604108复用探针；job140602604434备份核验7/7；job140602604510原厂10文件完整定位，artifact15211859421，app70七文件及EFS三文件（connectivity.json、dio_manager.json、iap2.cfg）。不重复旧7项、探针编译或旧库存扫描。新4新增从既有库存生成候选路径及只读哈希分类脚本，新5生成覆盖10个原厂参考文件的独立清单，测试缺失/重复/越界/错误证据范围，作为电脑端backup_verify输入；参考固件清单不等于车机实备或全部需备份清单，MIB镜像备份需先安全提取映射，禁止以此宣布备份恢复已完成。新6先查luka-dev/mib2q-carplay-rgi固定20c94f51c0f0ae88b154c12e4f604643ba23022c的GRAFT_LAUNCHER_LIFECYCLE及build_launch_candidate.py：异固件MU1316原厂服务管理器/launcher启动需完整生命周期，单独加载不证明回调；不使用其诊断绕过或地址。复用本工程R94及mu1005_efs_launcher_stage，已知launcher.btstack.exec、preCondition、onFailure，不重跑旧启动参数验证。新6仅查原厂framework/dsistartup/reset_bt.sh/connectivity启动恢复锚点；复用固定固件提取工具，不输出原厂配置内容。MIB upstream backupplus_system.sh核对为外部备份工具调用，非原子恢复证明，保留已有txn控制器。3路专项，无生产改动，无实车写入。新4/5/6并行，真实恢复安全门禁仍强制，33限定完成。NO INSTALL。

## 50. R105.187新4/5续攻，新6原专项保留
R105.186 run46027944302 job140604072945 SUCCESS，artifact15213230783（10项参考清单及只读脚本）；job140604072732 新清单5/5成功；job140604072721原厂启动链提取尚在进行（本轮提交前），不取消、不重提同一固件。复用GitHub MIB backupplus外部备份流程及已查证的LuKa生命周期资料，现有源码优先：R105.155 stat/fstatvfs身份探针及tools/review_txn/sha256.c（不依赖车机外部sha256sum），R105.186固定参考清单和R105.185 backup_verify。新4新增只读ARM身份检查，补/usr/sbin/mm-ipod、/lib/dll/ipod-drvr-iap2.so候选别名，按固定文件大小读取、检查SHA及同文件身份，不能从同哈希推断同inode；仅主机新三种情形与ARM编译。新5新增显式备份文件映射输入，避免猜MIB镜像目录或仅凭同名认定匹配，复用旧验证器，新增5项测试；必须先获得并安全提取真实备份，尚未验证全车备份/恢复。仅2个新任务及原第6项1任务，不重复已过。33限定完成，新4/5/6仍在攻关。NO INSTALL。

## 51. R105.188 用户要求100%状态及独立步骤补位
R105.186三路全部SUCCESS；第6步实际boot日志job140604072721 artifact15212129980找到framework.json、dsistartup.json、connectivity.json及reset_bt.sh，静态锚点而非启动运行。R105.187 run46028235774两路SUCCESS：job140605066968 22候选、同inode/不同inode/不同SHA三项主机通过及ARM编译，artifact15213520900；job140605067434映射5/5通过。新4/5的现有软件产物通过，真实车机路径/备份/恢复门禁尚未100%，挂起待一次只读/受控实车验证，不继续重复同一软件测试；第7原厂配置生效同样待实车，绝不提前写车。新6/8/9继续独立研发，任何真实加载/服务注册/RFCOMM连接均未证明。第8/9开工先查LuKa固定20c94f51c0f0ae88b154c12e4f604643ba23022c的wireless/graft/graft.c，RF_REGISTER/SDP_ADD/RF_SEND、event6及分段机制已存在，MU1316地址/arg3不可直接套用，复用本工程MU1005 inbound_r5/native_binding及R105.183已有type4协议。新增显式首连前ir5_enable_completion_ack和mu_bt_native_attach_ack入口，要求独立匹配同一FD的兼容daemon证明、仍保留真实owner/原厂配对检查、默认旧接口不变；没有实际调用者，不会自启。专项使用既有真实TCP57342测试夹具和event6（错误token拒绝、分段无提前ACK、末段唯一ACK），ARM编译；原厂回调非真实，不能当第8/9通过。第6仅组合已定位启动恢复文件证据与路径参考，不重新提取固件、不冒称运行通过；真实启动仍待攻关。两路新专项及生产源码触发原有自动回归（计划最多16路），提交前0路运行。累计33限定完成；新4/5/7待实车，6/8/9攻关，100%状态逐项明确。NO INSTALL。

## 52. 用户纠正：未解决问题不得挂起或转移后遗忘
2026-11-27 用户明确要求未解决问题必须继续处理，不得把实车依赖当作挂起、跳过或完成的理由。此规则覆盖第51节挂起安排，保留三项并行、最多20路及GitHub优先复用。当前优先新4/5/6，已开展新8/9的成果保留、自动测试自然结束，不将其当作替代4/5验收。各项记录100%与否、已通过证据、具体缺口、解决办法及实际验收条件；云端无法取得的真实车机证据明确标记阻塞，不能以模拟、静态、重复测试替代。新4需要实际文件身份/挂载及原厂恢复入口证据；新5需要当前车机真实备份、独立核验、可信恢复入口与受控恢复能力；新6需要原厂启动管理器实际加载及真实生命周期证据。准备尽量一次性只读采集与备份核验，必要的受控恢复验证须先确保供电、原厂独立恢复与用户同意，不能冒险写入。R105.188专项run46028842998两路SUCCESS，真实TCP+原厂事件夹具与ARM编译、静态启动边界组合已过；46028843008、46028843017 SUCCESS；统一回归46028843161查询时仍运行。不将任何一项标100%。累计33项限定完成，非实际功能百分比。NO INSTALL。

## 53. R105.189 用户批准原厂直用、MIB复用、独立目录最小改动
R105.188统一回归46028843161九路全SUCCESS，之前专项2路/集成4路/回调专项已通过；未新证明原厂实际加载。按最新用户指示不另写备份恢复引擎或启动管理器。复核Mr-MIBonk/M.I.B._More-Incredible-Bash固定3a2526a386f54ec7f49bbc32984ef7edb1106d33的backup.sh、backupplus_all.sh、ImageRestore.sh；复用用户已有MIB菜单，ImageMOD -b并非全镜像通用恢复命令。LuKa固定20c94f51c0f0ae88b154c12e4f604643ba23022c install_MoreIncredibleBash/mod/custom.sh及build_launch_candidate.py确认独立hooks、原厂路径复用，但该安装脚本适配MHI2Q而非AU57x MU1005，不直接安装。原厂/eso/bin/reset_bt.sh已证存在但只是蓝牙重置。新6优先复用已有mu1005_efs_launcher_stage及原样参数转发，拟独立目录/mnt/app/root/au57x-wireless（尚未验证）避免替换原厂二进制，不再造新启动器。新4/5/6整合既有R105.187原生身份/哈希、R105.173配置身份、R105.172固定FD观察到一个只读SD包，不重复编译，复用原有MIB/SSH操作，不能自动改车。仅1路新包整合，无需重复已过测试。真实文件身份/恢复能力/工厂启动生命周期缺口保持，未100%，33限定完成。NO INSTALL。

## 54. R105.190 用户同意研发与实车验收分开、直接使用固件
用户批准原厂固件已明确的路径/地址/接口直接复用，不再要求重复现场证明才能推进软件开发；最终统一实车验证。R105.189 run46029598274/job140609740965 SUCCESS，artifact15213352546（15263字节），已复用通过的两只ARM工具，整合包完成。新4按固件路径/入口及软件只读工具范围收口，新5按现有MIB备份恢复方案与离线核验工具范围收口，累计35/66限定软件/静态等范围完成；实际路径、恢复入口、当前车机备份及故障恢复验证仍作为首次实车写入的强制门禁，不宣称已100%实车完成。新6继续解决软件接入；复用LuKa独立hooks/原厂launcher思路及已有tools/mu1005_efs_launcher_stage.py，R105.190只将原厂单字段候选路径迁移到/mnt/app/root/au57x-wireless/，原厂exec、argv、preCondition、故障处理等语义保留，不替换原厂二进制。只从固定原厂EFS提取实际配置，不重复app70提取，验证新布局精确单字段可逆；不公开原厂配置。并行复用R105.188已通过的原生预加载诊断库与native binding库成品（默认不激活），不重编或混淆构造函数与真实SDK owner。尚缺原厂实际生命周期接线；不能称无线已启动。2路新专项，保留新8/9已有ACK证据，下一批新6/8/9软件攻关。NO INSTALL。

## 55. R105.190验收及R105.191现有原厂事件线程接线
R105.190 run46030265624两路SUCCESS：job140611977072 artifact15213623114，实际ARM导出mu_bt_native_attach_ack；job140611977276 artifact15212774993，精确原厂配置单字段可逆，候选SHA cc5ee2e8988aca261f02d43aab9d71e3b3dac90adb0e0f66cd249e110ec5d79c。新4/5软件范围完成不重跑。继续新6/8/9，复用上游LuKa已有graft/原厂startup生命周期及本工程BT_QUEUE_BOOTSTRAP_CN、bt_notify_bridge、au98_drive、native_binding，不重新实现SDK。发现R105.188新ACK入口尚未由既有owner dispatch调用，且daemon witness与owner context不同。R105.191新增显式ACK选项（NULL仍走旧协议），在现有notify/queue owner驱动中同时核对准确FD的daemon身份与type4能力，由现有au98_adopt传给native attach；独立同步借用context不进入长期owner/回调存储。原厂owner/配对/MFi/有线抢占约束不变，真实daemon能力provider和真实startup调用者仍缺，不自动激活。不重做已通过路径或完整协议测试，只新增真实TCP夹具验证独立context、拒绝和显式ACK接入及ARM编译，预计1专项加自动回归（不超20）。35限定完成，新6/8/9未100%，NO INSTALL。

## 56. R105.191验收及R105.192组合入口（用户批准MIB/绿屏、原厂有线模拟、GitHub复用）
R105.191专项46030667219成功，统一46030667004九路、回调46030667055一路、集成46030667054四路成功；上游46030667158四路中oem-contracts失败（job140613328017，test_r105_owner_nonblocking_gate.py:38仍要求旧函数名），合计19路18成功1失败。R105.192更新测试以检查新adopt_stock_common与ACK/legacy两条完整调用链，仍禁止socket/connect/等待poll，socket_ready只允许零超时；不删除安全约束。用户批准组合MIB/绿屏脚本入口、原厂有线模拟、GitHub成熟无线graft。复用MIB已核实custom.sh入口，LuKa固定20c94f51c0f0ae88b154c12e4f604643ba23022c的启动与有线接收器设计，R106/R44有线数据夹具和R105.146现有原厂配置试验，不另写有线接收器、不伪造MFi、不让模拟占用实车有线会话。新6实际缺口为已有bt_trace_attached_take私有latch到bt_startup_watch及mu_bt_queue_bootstrap的桥接，新增通用trace source和QNX worker入口，ready只传布尔通知，不把OEM指针带出lease；释放后由原有queue bootstrap重新获取lease、解析真实GOT及started对象。实际原厂启动caller、真实生命周期lease及daemon能力provider仍未完成；MIB脚本能启动独立进程但不能直接在原厂进程内创建真实SDK owner，不宣称实际启动。新8/9沿用R105.191现有owner dispatch和ACK，未重复编译已通过的独立布局。专项1路加自动回归，保留20上限。35限定完成，NO INSTALL。

## 57. R105.193 四路线并行准备、顺序回退方案（用户批准）
R105.192四个workflow共18路全SUCCESS，旧规则修复通过。用户确认原厂配置、MIB独立入口、原厂有线接收器复用、原厂蓝牙graft四路线准备，最终成功即停止、失败恢复后再继续、全部失败回讨论。四路线不是四套已可运行无线实现；2/4可能组合，3复用下游。现有R105.146/R105.190/R106/R44/R105.191/192直接复用，不重写已有功能。此次新增lab四路线选择/恢复顺序，复用既有txn_supervised_trial及supervisor_fixture在同一个临时目标目录、每路线独立journal、独立原厂文件内容/属性校验后才允许继续；成功即停止，未知/恢复失败/崩溃即阻塞，6个新组合测试。不修改车机/原厂二进制、不生产自动刷写工具；实车adapter、原厂启动owner/lease、daemon能力、真实无线会话仍缺。实际MIB备份、独立恢复及写入门禁保持，不能无人值守重启试错。1路新测试，不重复18路。35/66限定完成，新6/8/9均未100%。

## 58. R105.193验收及R105.194上游真实daemon协议差异
R105.193 run46032818633 job140620634723 SUCCESS，6/6组合测试，artifact15215186473；不重复恢复流程测试。新6/8/9仍未100%。复核LuKa GPL固定20c94f51c0f0ae88b154c12e4f604643ba23022c wireless/daemon/src/lib.rs:28-29, FrameDecoder及serve_stream_with_receiver，实际成熟Rust daemon仅支持1/2/3，R105.188/191本工程type4若直接发送会导致上游断开。R105.194新增精确上游源码适配，仅显式--mu1005-ack模式允许同代单个待确认TX，错误/重复/超时停止，默认旧协议不启用；不改MFi、接收器及AP。实际编译上游Rust并通过真实TCP与合成无线端测试ACK/不匹配/缺失/legacy，不重做整个QNX环境。实际daemon FD能力提供者、原厂SDK lifecycle和真实无线仍缺，ARM构建和原厂接入未证。1路专项，35限定完成。NO INSTALL。

## 59. R105.194验收及R105.195真实C/Rust联合移植
R105.194 run46033208663/job140621972391 SUCCESS，artifact15214369795，实际Rust daemon在真实TCP上正确接收ACK、拒绝错代及旧模式ACK、超时退出，日志MU1005_DONOR_ACK_REAL_TCP_OPT_IN_AND_LEGACY_PASS_NO_OEM。R105.195复用同一LuKa GPL固定20c94f51c0f0ae88b154c12e4f604643ba23022c，合并既有adapt_live_ap、adapt_receiver_unix、adapt_mfi_sequence及已过ACK适配。复用原bridge_peer C代码只增显式ACK/receipt核验，实际C ingress/IPC/socket pump连接合并后的Rust daemon，两TCP四代marker/SYN及分段SDK完成夹具，不重复单项测试。另用已有Rust QNX工具链构建完整合并ARM程序，只编译新组合，不重跑历史PHDR或QNX启动夹具。QNX编译不等于真实运行，保持默认真实硬件认证、不启动车机。新6实际启动/生命周期及daemon FD身份provider未完成；新8/9真实注册/无线传输未完成，35/66限定完成。2路专项，NO INSTALL。

## 60. R105.195实际验收及停止重复实验决定
run46033436233两路SUCCESS：job140622748616 artifact15215541990，实际合并Rust守护程序+C RF ingress/socket，在两个TCP连接上四代均35 bytes/8 fragments marker/SYN PASS，ACK receipt夹具通过；job140622749269 artifact15215267512，完整合并QNX ARM ELF32 release编译成功。不可将合成SDK事件当作实车RF、MFi或无线认证。已通过组合构建、恢复顺序、原厂配置单字段、协议ACK不再重跑。剩余共同阻塞是原厂真实生命周期启动/关闭互斥、有效配置实际消费、真实owner/daemon admission。用户已批准四路线按安全顺序尝试；优先原厂单字段配置路线复用R105.109/119/146（iap2.cfg保持原样），实车实际MIB备份、独立恢复入口及路径/写权限验证完成前不得写；其他路线继续复用现有组件，不再新增无实质突破的外围模拟。此次仅记录已实证验收，不伪称新代码或增加总完成数。35/66限定完成，新6/8/9未100%；NO INSTALL。

## 61. R105.196 用户实车日志直接复用
用户上传001(7).zip，已实际解压核对20快照及路径、挂载、argv。详见docs/R105196_R106_LIVE_PATH_CN.md。此前观察工具不再要求单独实车运行；实际缺项留到最终同次验收。根据真实pidin ar布局修复已有trial.sh进程判断，查询失败不再误当有线未运行，实际命令名而不是任意token，单次采样。专项1路（新格式），旧5路只手动，不重跑已验证固件生成恢复。没有原厂真实配置消费、owner或无线证据，35/66限定完成，新6/8/9均未100%。NO INSTALL。

## 62. R105.197 用户批准15项集中决策，补齐队列dispatch生命周期边界
用户回复1，15项推荐全部批准，详情docs/R105197_USER_DECISIONS_CN.md。今后已定方向无需重复问；共同核心问题仍要完成真实provider，不可用选择代替证据。R105.196两workflow SUCCESS。修复真实代码：原有producer持lease但AO dispatch未持有，现mu_bt_pulse_dispatch_leased非阻塞取得同一lease并覆盖owner/service验证及SDK dispatch；关闭中失效则跳过，start/retire owner检查在lease内。明确真实原厂stop/destruction provider尚未实现，非阻塞lease禁止假证据。1项新增专项加自动回归，35/66限定完成，新6/8/9未100%，NO INSTALL。

## 63. R105.197验收与R105.198真实关闭边界定位
R105.197三workflow 46035405883、46035405819、46035406005共14路SUCCESS，包括实际ARM dispatch lease编译；不再重复测试。新6/8/9的共同阻塞仍为实际原厂关闭与启动互斥提供者。R105.198只查真实原厂btstack的OSAPI stop/cleanup、BluetoothServices start/stop的直接调用、函数体内调用及可用导入重定位（ActiveObject stop/destructor、mutex/join），寻找可复用的统一入口；复用已有精确固件归档和harman-f 64a44fe23dbaa08771440d69642d2874d7028295 GPL3.0、qnxmount 0379c064975d5bbe3595ac7e3d149ea789406794及原有Binary解析，不重新扫描配置/线程等旧问题。不公开原厂二进制、不自动生成patch，不把直接调用候选当作所有间接调用覆盖证明。1路专项，35/66限定完成，新6/8/9未100%，NO INSTALL。

## 64. R105.199 用户授权按最多20路一批全项目并行
用户要求剩余所有问题全部开始，20组一批，失败修改复测。沿用15项既定决定；每批包括自动触发回归在内预算20，不强行凑数，不重复已通过单项，不把实车专属项目模拟为完成。R105.198 run46035836698 SUCCESS但STOP_RELOCATION_CANDIDATES=[]，两个真实OSAPI stop调用已证，未闭合间接关闭路径；不得假造统一GOT入口。第6/8/9共同生命周期阻塞保留并行攻关。R105.199本批横向处理第14原厂MFi：实际cp_stock_mfi wrapper在原厂调用返回及释放后重新核对同一会话/有线优先，撤销时拒绝输出并失效，不改OEM认证；第21原厂receiver：实际SI登记完成后读取全部原有ID与新ID，检查上限和最终owner，旧记录被替换/撤销则不报成功；复用现有适配和原厂ABI，无新协议实现。新增4个MFi撤销情形+3个SI登记边界，预计加自动9+4回归共20路，以实际GitHub run为准。其他未完成软件项新6/8/9/11/12/17/18/22/23/24/35按依赖推进；不将全部未启动的研发称已提交。真实手机/认证/无线、MIB独立恢复及实际配置验证仍缺。35/66限定完成，NO INSTALL。

## 65. R105.199验收/R105.200第二批
R105.199两workflow 46036363639、46036363462共16路SUCCESS，新增MFi 4/4、receiver 3/3及联合9/9，不虚报20。R105.200修复实际启动watcher对非0/1 trace或lease返回的处理，负数不能当作授权，错误清除待启动窗口并拒绝原厂读取；修复现有IPC decoder同一read中终止帧后附带部分header被误认为成功、RX/TX计数最大值回绕，拒绝歧义状态并关闭。新增6项定向负例，自动回归预算≤20；AP生产修改留下一批以避免触发额外9路。继续复用R105.195及现有原厂接口，不新增协议。第6/8/9/11/12仍缺真实启动和接线，第14/21已通过软件边界测试但真实owner未绑定。35/66限定完成，NO INSTALL。

## 66. R105.201 第三批同一会话热点参数
R105.200三workflow46086291583、46086291605、46086291623共19/19 SUCCESS，保留结果。R105.201按用户20组预算并行，实际修改ap_refresh.c：读取前固定epoch/generation/band/bootstrap，读后变化拒绝并擦除；IE query前后再验证owner；IE设置后重复原厂AP快照并比较全部参数，改变拒绝提交且保留IE lease供真实恢复。复用LuKa 20c94f51c0f0ae88b154c12e4f604643ba23022c GPL及既有原厂uaputl/5703适配，不新造无线协议。新2组专项+原4组AP owner+1组正常/已有IE集成+自动9+4预计20；旧R105147未改ap_live实现不触发，R105149全8组手动保留，自动仅正常和既有IE，避免重跑无关及超预算。真实AP owner、无线参数发送未完成，新17/18未100%，35/66限定完成，NO INSTALL。

## 67. R105.202 用户确认20路滚动补位
每次检查立即补足有意义的独立任务，不必等整批全部完成；在运行、排队、自动回归及待汇总合计最多20。用户要求尽量维持20路，不为凑数重复成功测试；真实依赖或无新增代码时留空，不伪称持续后台自主研发。R105.201本次检查18成功、1运行、1待汇总。立即补充R105.202：第21/22/24相关实际AirPlay receiver wrapper在原厂set_property、监听回调、stop后重新确认真实owner，开始/关闭调用前再次核对；失去owner不报成功，保留未确认关闭责任。不重新实现原厂接收器/Bonjour，实际接口/mDNS/owner仍缺。新3组定向+自动9+4预计16，留2个旧任务合计≤18，以实际队列为准。35/66限定完成，NO INSTALL。
