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
