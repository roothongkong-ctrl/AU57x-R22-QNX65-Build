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

**下一任务：先核实 R105.156 的 reader-targets / boot-path 两路实际结果，再决定后续修改。** 当前连接器只返回 PR 触发的 commit runs，未找到此次 push workflow；不能认定运行完成或失败。可在 [Actions 工作流](../.github/workflows/r105156-config-reader.yml) 对应的 Actions 页面查询；若权限允许，按真实 run ID 检查 jobs、必要日志和 artifact。不重复触发已成功任务。

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
| R105.156 | 本轮确认源码已提交，push run 最终结果未核实 | [报告](R105156_CONFIG_READER_CN.md) / [commit](https://github.com/roothongkong-ctrl/AU57x-R22-QNX65-Build/commit/88b0ceace9e8da91b03f33188eb61f9271a2cc95) |

R106 实车已有：原厂进程、2.4GHz AP association、有线 CarPlay 视频音频等证据；没有真实无线 CarPlay、真实无线 MFi、真实 5GHz 的成功证据。

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
