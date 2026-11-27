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
