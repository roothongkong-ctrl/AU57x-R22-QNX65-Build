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
