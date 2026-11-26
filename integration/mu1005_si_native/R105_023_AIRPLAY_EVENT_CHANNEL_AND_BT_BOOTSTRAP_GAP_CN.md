# R105.023｜AirPlay event-channel 单读者机制与 Bluetooth bootstrap 的 AU57x 适配分析

日期：2026-11-26
目标：MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
参考：congobg/MHI2Q_WiFi_CarPlay Technical Reference（公开 README 最近更新 2026-10-27）
本地基线：R105.021、R105.022、inner_iap2、inner_airplay、OEM_AIRPLAY_ABI_CN、R99、R103
状态：SOURCE_REVIEW / COMPATIBILITY_GAP_ANALYSIS / NO_INSTALL

## 1. 外部项目最新资料给出的关键实现细节

参考项目在 MHI2Q QC 的报告不仅是一个 AirPlay sender ABI。它明确描述了以下真实运行机制：
- RFCOMM bootstrap：btstack 中注册 RFCOMM server channel 4、iAP2 与 CarPlay SDP 记录；Bluetooth 侧守护进程完成 MFi authentication、identification、Wi-Fi credentials 与 CarPlayStartSession。
- Wi-Fi session：手机加入车机 AP，连接 stock dio_manager 中的 AirPlay receiver。
- iAP2 inside AirPlay：stock iAP2 client（Cinemo）第二次连接手机；车到手机的 iAP2 packet 通过 event channel 的 iAPSendMessage，手机到车通过 control channel 的 iAPSendMessage。
- Event channel pump：参考项目发现 stock receiver 只在等待自己发出的 event-channel request reply 时读取 event socket。hook 必须在 stock 线程未等待 event socket 时继续读取；自己发出的 request reply 立即消费，stock 的 reply 暂存并在 stock 等待时再报告 readable。由于 event reply 没有 CSeq，stock 与 hook 的 request 必须按 wire order 排队；只允许一个 reader。
- Session lifecycle：无线 dio_manager on-demand 启动；Bonjour/mdnsd 与 AP 实际运行 channel 要同步；iAP2 inside AirPlay 成功后再按 stock 流程释放 Bluetooth；USB 与 wireless 的 dio_manager 只能有一个，USB wins。
- 5092 特别项：stock iAP2 client 在 USB service 路径才会启动；无线 session 需要让原厂 iAP2 client 在 AirPlay receiver ready 后启动。参考项目为其 5092 构建表专门处理，不可直接复制地址。

来源：
https://github.com/congobg/MHI2Q_WiFi_CarPlay/blob/main/docs/README.md
https://github.com/congobg/MHI2Q_WiFi_CarPlay/blob/main/docs/Wireless%20CarPlay%20on%20MHI2Q%20QC%20%E2%80%94%20Technical%20Reference.md

## 2. 对 AU57x 现有代码的静态核对

### 2.1 已存在但不能代表真实 event-channel 接通
- inner_airplay.c 绑定 AirPlayReceiverSessionSendCommand，支持将本地 iAP2 packet 包成 iAPSendMessage 并调用原厂 sender-side API；该符号方向是车到手机的 command send，不是手机发来 command 的接收 hook。
- inner_airplay_control() 可解析已解密的 {type, params:{data}} CFDictionary 并交给 inner_iap2；但 header 明确要求调用者在 original control dispatch、正常 handler 之前调用。当前未证明 AU57x 存在该调用者或已经注册此 callback。
- inner_iap2.c 和 inner_socket.c 处理 packet relay、epoch、peer credentials 与 partial write；它们不拥有 AirPlay TCP event/control socket，也没有 stock receiver event queue 的请求/回复 wire-order仲裁。
- OEM_AIRPLAY_ABI_CN.md 明确写明尚未实现原厂 dispatch owner 注入、真实控制回调绑定、mm-ipod plugin 或加密。该限制仍然成立。

### 2.2 Bluetooth bootstrap 的真实缺口
- R99 证明原厂 bluetooth ELF 中存在 bluetooth.enableIap / iapEnabled / SERVICETYPE_IAP2 等名称；ipod-drvr-iap2.so 含 BluetoothiAP2Link、BluetoothTransportComponent、WiFiTransportComponent 名称。
- 但 R99 明确未证明启用配置后 RFCOMM service 会被注册，也未给出 AU57x 真实 RFCOMM channel、SDP record、BlueSDK 回调或 MFi bootstrap 的运行闭环。
- R103 复用 LuKa iAP2 link engine 并通过主机 TCP lab 验证 framing；其自身文档明确说明 AU57x btstack 线程注册入口、SDK callback 与 channel 尚无实车证据，Rust host test 不等于 QNX target。
- 因此不能把外部参考的 RFCOMM channel 4、SDP UUID、127.0.0.1:57342、hook addresses 或 btstack build offsets 复制到 AU57x。

## 3. 真实数据面必须按双向路径拆开

车 → 手机：
1. AU57x stock iAP2 client 产生一个完整 iAP2 packet。
2. packet 进入 AU57x stock iAP2 transport_send_pkt / transport callback。
3. 适配层将 packet 转成 AirPlay event-channel iAPSendMessage command。
4. event-channel pump 负责发送、读取、区分 stock 与 bridge 的 reply，并按 wire order 交还唯一 reader。

手机 → 车：
1. AirPlay control channel 收到加密 payload。
2. stock AirPlay receiver 完成解密和 binary plist/CFDictionary decode。
3. 在原厂 dispatch owner 中识别 iAPSendMessage，取出 CFData。
4. 将原始 packet 交给 stock iAP2 client transport_receive。
5. 原厂 iAP2 driver 继续做 detect、link、authentication、identification 与 MFi 状态机。

两条方向都必须在同一真实 AirPlay session、同一代次/epoch 下闭环。不能把 sender-side API 成功排队当作对端已 ACK；不能伪造 MFi authentication 或 Identify 成功。

## 4. 对当前 AU57x 代码的结论

| 子系统 | 当前状态 | 结论 |
|---|---|---|
| iAP2 packet parser / relay | 已有静态实现与 host tests | 可复用，不需重写 |
| AirPlay sender-side API ABI | 已由 AU57x libairplay 静态反汇编记录 | 仅 sender-side 证据 |
| AirPlay control receive callback | 未定位/未绑定 | 当前最大阻塞 |
| AirPlay event-channel single-reader pump | 未实现 AU57x 原厂 socket owner 接入 | 必须先取得 socket/queue owner |
| RFCOMM bootstrap | 原厂配置字符串/驱动组件符号存在 | 未证明 channel/SDP/MFi 连接工作 |
| Wi-Fi AP handover / Bonjour | 有 interface helper 与部分 DIO 生命周期工具 | 未证明 AP owner、运行 channel、mdnsd 与 session offer 的 AU57x 时序 |
| Stock iAP2 client wireless start | 有 driver/transport 静态证据 | 未证明 MU1005 无线 session 中启动方式和 USB 仲裁 |
| USB coexistence | 有设计约束与 CPRX lifecycle code | 未证明 AU57x stock owner 的互斥行为 |

## 5. 下一次真正的开发任务（按依赖顺序）

1. 从 AU57x dio_manager 导入表和 callsites 精确定位 AirPlayReceiverServerCreate、session start、event client 与 control/event socket 的创建和读写 owner；不再只查 sender function。
2. 在 AU57x libairplay.so 与 dio_manager 中追踪 control channel 解密后的 plist dispatch；找出接收命令 callback、queue/thread、参数对象生命周期、返回码及 stock handler 的调用顺序。
3. 追踪 event channel 的 socket fd 从 accept/connect 到 select/poll/read/write 的所有权，恢复 stock request/reply 协议和 reply order；先做只读调用图，不先注入 hook。
4. 追踪 AU57x ipod-drvr-iap2.so 的 transport_receive / transport_send_pkt 调用方，定位 mm-ipod 插件接口实例和进程间 IPC，确定 stock iAP2 client 如何连接到 AirPlay transport。
5. 追踪 AU57x bluetooth 配置加载点、BluetoothTransportComponent 实例化与 BlueSDK RFCOMM API 调用，确认服务 channel、SDP、MFi auth 与 Wi-Fi credentials 的 owner；不照搬 QC 数值。
6. 追踪 AU57x AP start/config/channel query、station events、Bonjour/DNS-SD 与 dio_manager lifecycle；运行 channel 必须在发给 iPhone 的 credentials 中取自 active AP，而非过早读取静态配置。
7. 仅在 owner/queue/fd 证据闭环后设计 event pump：单 reader、wire-order queue、stock replies 暂存、bridge replies 即时消费、队列上限/背压/超时/epoch fence 与断线清理。
8. 在 host 测试中加入交错的 stock 与 bridge event requests/replies、无 CSeq wire-order、最大 iAP2 packet、延迟 reply、partial read/write、队列满、session drop 与重连；再复用既有 QNX ARM build pipeline。

## 6. 最小缺失证据清单（只在本地原厂文件里取，不重复索取已交付项）

优先从已经交付的 app.img / mifs-stage2 / K1006_2 firmware tree 和已提取 ELF 中继续，不要求用户重新上传已有文件：
- 原厂 dio_manager 完整反汇编/符号导入导出表（现有样本按哈希核对）；
- 原厂 libairplay.so 完整反汇编与 event/control channel 相关交叉引用；
- 原厂 ipod-drvr-iap2.so 的 transport_receive / transport_send_pkt 调用图；
- mm-ipod 与 ipod-transport 插件的加载路径、iface table 和跨进程消息契约；
- bluetooth 配置文件的真实路径与加载者，以及 BlueSDK RFCOMM/SDP 注册 API 调用点；
- AP manager 与 Bonjour 的启动脚本/服务配置和运行接口查询点。

如果这些精确二进制/反汇编不在当前 GitHub 工作区，应从此前已交付的私有本地原厂文件重新提取；不能声称已经在 GitHub 云端看到或分析了未上传的私有原厂二进制。

## 7. 本轮结论

外部 MHI2Q 成功项目提供了关键实现范式：Bluetooth bootstrap + Wi-Fi AirPlay + iAP2 inside session + event-channel pump + USB/wireless single dio_manager ownership。对 AU57x 最重要的技术难点是 AirPlay control receive callback 和 event-channel 单 reader/无 CSeq reply ordering，而不是再写一个 iAP2 parser。

本轮仅完成公开参考资料的技术提取与 AU57x 源码边界核对，没有修改原厂二进制，没有接通真实 AirPlay callback，没有执行车机或 iPhone 测试。

installable=false / safe_to_flash=false / vehicle_verified=false / mfi_apple_verified=false。