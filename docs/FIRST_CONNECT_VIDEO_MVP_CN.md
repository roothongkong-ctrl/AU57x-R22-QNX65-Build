# 第一阶段：先连上、先显示（R105.046，NO INSTALL）
用户决定首连只要求 iPhone 无线 CarPlay 建连并显示，2.4GHz 优先；音频、电话、Siri、完整控制、重连、5GHz 优化后置。最终10步不变。R105.045 SI run 45907011058、unified run 45907011075 均 success。
本轮复用现有 R84 SI/DIO AF_UNIX、iAP2 4301 ACK、原厂 AirPlay 及 MFi，不另建协议。
* 修正原 pipeline 将 START_REQUESTED、AIRPLAY_SESSION、INNER_IAP_AUTH、MEDIA_ACTIVE 全部绑在 media==7、四路 mask==15 的错误顺序。分阶段独立确认 4301 ACK+listener/Bonjour、实际会话、真实内层 MFi；内层认证完成后才允许实际 BT close，后续无需等音频/MMI/方向盘。未伪造任何认证。
* 新增独立 first-video observation，仍使用同会话原厂 op11 video bit 和 SI video 证据，既有全功能 MEDIA_ACTIVE 判据不变。暂保留现有原厂媒体 flag=7 的协议约定，不猜测 0/4/5 的物理含义；不代表实际 LCD 已显示。
* 现有 cp_cloud_confirm_bootstrap_auth 已按真实绑定 AP channel 选择首连频段（channel 1..14 为2.4GHz），无需重复修改双频逻辑。实际 AP/owner 启动尚未绑定。
* 修改现有真实 Unix socket host fixture 的正常主链：4301 ACK→内层认证→BT close→仅 video→完整媒体。其 OEM/手机证据均为 fixture；QNX ARM 编译并不证明车辆连接。
未新增通用安全模块；真实蓝牙启动和授权、原厂 MFi signer/daemon 绑定、Wi-Fi association、原厂 SI/DIO AirPlay 与 video 回调尚未实际运行；不可安装，固定10步未验收。首连四线并行，以这些真实接点为优先。

## R105.047
R105.046 native-library job 140207246275 success；SI run 45907376247 success；unified run 45907376230 authentication host fixture assertion line1327 failed. 原因候选：客户端分段状态读取可能在每轮第一次 tick 确认快照，而第二次 tick 开始新查询并撤销快照，测试只检查第二次。现改为每次 tick 后立即检查 video，不改变媒体证据或授权。另修正现有 inner_session HTTP dispatcher：先使用已验证 library 判定是否 iAPSendMessage，再执行无线 owner 检查，非本协议原厂 requestUI 等始终由原函数处理，避免原厂 UI 受 relay 失效影响。现有测试加入该正常原厂回退路径。新 CI 待核实；真实原厂装载、授权、RF、Wi-Fi/AirPlay 仍无运行证据，NO INSTALL。

## R105.048 首画面模式继续整合（SI + DIO）
R105.047 unified run 45907707475 九个 jobs 全部 success（authentication 140208332410），SI 45907707437 和 iAP2 45907707426 success。原先 SI 独立激活适配器 si_mu1005_cprx.c 仍要求 mask==15，本轮新增显式 mu_si_cprx_init_first_video，保持原全功能入口 mask15 不变，首连入口只要求同一原厂会话实际 inner MFi、media7、video bit。DIO 现有 op11 将可选音频/MMI/方向盘的未知(-1)表现为无对应确认位，不再拒绝完整查询；video 未知仍 RETRY，原厂认证未知仍拒绝。不改变协议格式。真实跨进程 fixture 增加 first-video 模式，使用当前 DIO 源码，原 R85 pin 的全功能测试保留。未声称实车已显示，最新 CI 待验。仍需生产原厂运行时调用者，NO INSTALL。

## R105.049 正常连接 owner 循环合并
在现有 cp_cloud_pipeline.c 增加首连入口 cp_cloud_drive_first_connect，串联 cp_s4_tick、真实认证确认、已验证热点的 band 选择/准备、真实手机 association、原 DIO endpoint bind、同一 CPRX START/status、首视频判定。此前调用方必须在 association 前自行保持 5702/5703 pump，且逐项驱动多个状态；现在连接中也持续处理原 iAP2，等待真实 AP 状态，不自动制造 association、不重复启动 DIO。保留原有完整媒体模式、MFi、原厂有线优先与所有权，实际 RF bytes 仍须由真实传输生产者处理；不新增线程、不启动硬件。已有 R84 fixture 改用同一入口驱动 2.4GHz 等待关联→DIO START→首视频，确认无音频/控制也能报告 first-video；实际 OEM/手机仍是 fixture。R105.048/049 云端结果待查，NO INSTALL。真实启动调用者与车载原厂接口仍缺，不计固定10步完成。

## R105.050 MFi 与真实 RF generation 接入
R105.048 两条 CI 与 R105.049 两条 CI 均 success。现有 R44 stock MFi 绑定必须提前给出 epoch/generation，真实首连无法依赖调用方在 RF CONNECT 前猜测 generation；本轮在现有 cp_step04_session 增加可选 prepare_stock_mfi，保存借用的已验证原厂库与 owner，在真实 R14 CONNECT→iAP2 SYN/LSP/ACK→首个 CONTROL 数据到达后，使用实际 link.generation 调用既有 cp_stock_mfi_bind / cp_auth_init。必须通过既有原厂库/当前 owner/控制链路/有线空闲及真实 AA00-AA05 认证，未跳过任何必要认证。旧显式绑定仍可用。修改 R90 真实 TCP fixture 走该正常延迟绑定路径，证书和签名是测试桩，非 Apple 真认证；没有生产调用者与真实 RF 绑定，NO INSTALL。最新 CI 待查。

## R105.051 保留早到的真实 4300 通知
在既有 R44 CONTROL dispatcher 内，已完成原厂 MFi/身份的 4300 先使用现有 cp_availability_wireless_4300 解析器记录真实手机可用状态，然后交给现有 pump；不再因 AP 暂未就绪或 pipeline 尚未启动而丢掉手机通知。4e0d 的可用状态也按真实认证帧记录。发送 4301 仍必须经过原厂认证、真实 live AP、receiver ready、独占 owner 与 TX slot，不伪造握手或就绪。复用原有测试，将 4300 放到 5703 ACK 未到且热点证明暂未就绪时，确认恢复后不需手机重发即可走原 4301 路径。生产 owner 尚未绑定，云端测试待查，NO INSTALL。

## R105.052 真实 TCP 监听入口
R105.051 unified 45908512907 与 SI 45908512904 success。既有 R98 btstack worker 连接 127.0.0.1:57342，C R44 socket_io 原只提供 adopt 已接受 fd，缺少对应 listener。新增实际 nonblocking bind/listen/accept（不启用 SO_REUSEPORT），在独立 peer proof 后交给原 socket_attach；只允许一个消费者，不与 LuKa daemon 并行占端口。复用 R90 真实 TCP→SYN/LSP/ACK→AA00-AA05 fixture，改用实际 listener API（不是测试手写监听），认证仍为测试桩。此接口尚无生产启动调用者，未执行原厂 RF 注册及真实手机握手，不计第1步完成，NO INSTALL。首次目标剩4项：BT/iAP2/MFi、2.4GHz、AirPlay/画面、联合及测试包。

## R105.053 首连蓝牙/iAP2 调度合并
R105.052 SI 45909012960 success；unified 45909012956 在本轮检查时 in_progress。现有 R89/R87/R44 已能处理真实 TCP bytes，之前需要调用方在 CONNECT 后分别 bind/ops/init/acl_up/tick。新增原 remote 模块的 cp_wcp_r45_bootstrap_tick：同一 socket owner poll，等待实际 CONNECT，使用真实 generation 初始化已有 R87，继续原 SYN/LSP/ACK、MFi AA00-AA05；只有 cp_s4_authenticated 才返回 OK 供既有 cp_wcp_begin_cloud 接管。复用现有 R90 真实 TCP fixture 验证调度，不另建协议和安全模块。保留既有真实 OEM proof，不伪造手机认证。尚未绑定原厂生产启动器/配对授权及真实 MFi signer，NO INSTALL，第1步未完成，固定剩余4大步。
