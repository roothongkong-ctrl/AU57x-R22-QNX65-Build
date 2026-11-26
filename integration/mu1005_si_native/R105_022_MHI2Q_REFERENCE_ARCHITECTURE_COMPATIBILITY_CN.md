# R105.022｜MHI2Q 无线 CarPlay 成功方案对照与 AU57x 适配边界

日期：2026-11-26
目标平台：MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
外部参考：congobg/MHI2Q_WiFi_CarPlay，README（截至 2026-10-27）
状态：REFERENCE_REVIEW / STATIC_COMPATIBILITY_ONLY / NO_INSTALL

## 1. 外部成功方案的关键架构（来源事实）

congobg 项目 README 明确记录其目标为 MHI2Q QC、Harman 2016、APQ8064、QNX 6.5 ARMv7、固件 3638/MU0917 与 5092。作者报告在 5092 上已实车验证无线会话、屏幕、控制、iAP2 inside AirPlay session；蓝牙 discovery/pairing、Wi-Fi handover、session start；音频、通话、Siri、输入、HMI 与 cluster map。README 还列出若干后续修复仍待车测，不能把整个清单概括为全部已验证。

该项目明确的核心数据路径：
1. Bluetooth 只负责发现/配对和 bootstrap，短暂 iAP2/RFCOMM 会话交换 Wi-Fi 凭据并触发 CarPlay。
2. Wi-Fi 承载 CarPlay/AirPlay 会话，使用 stock dio_manager / libairplay receiver。
3. 车机原有 iAP2 client 在 AirPlay session 内再建立第二条 iAP2 会话。
4. 这条 iAP2 会话的数据包以 AirPlay event/control channel 中的 iAPSendMessage 命令传输。
5. 无线会话内 iAP2 建立后，手机可断开 Bluetooth iAP2，车机按原厂行为释放 Bluetooth。
6. 项目使用 stock btstack 加 RFCOMM hook、无线守护进程、AirPlay receiver hook、HMI virtual device 与脚本编排。

README 还报告 stock receiver 210.81 本身不认识 iAPSendMessage；参考实现由 hook 在相应 event/control 通道处理该命令，而不是假定 stock libairplay 原生支持。

来源：https://github.com/congobg/MHI2Q_WiFi_CarPlay/blob/main/docs/README.md

## 2. 可迁移到 AU57x 的概念（不是二进制兼容结论）

| 参考架构概念 | AU57x 当前工程可复用的部分 | 仍需 AU57x 自身证据 |
|---|---|---|
| Bluetooth bootstrap / RFCOMM | 已有 BlueSDK/iAP2 owner 审计、SI/DIO 控制面 | AU57x btstack 插件注册、RFCOMM channel/SDP、真实连接回调 |
| Wi-Fi AP / channel handover | 已有 Wi-Fi interface helper、CPRX 生命周期和网络状态抽象 | AU57x AP manager、接口名、channel 获取时序、station event API |
| Stock AirPlay receiver | 已分析 AU57x libairplay ABI，存在 AirPlay command sender 适配代码 | AU57x event callback owner、session 创建/销毁、dispatch queue 与 hook 点 |
| iAP2 inside AirPlay | inner_iap2 FF55/FF5A parser/relay 与 inner_airplay adapter 已有 | AU57x stock iAP2 client 启动时机、端点和 AirPlay event/control 双向桥接 |
| HMI offer / device state | SI/CPRX owner 与生命周期模型 | AU57x HMI 虚拟设备/设备状态事件的真实原厂接口 |
| USB coexistence | 已有有线优先/恢复的设计约束 | AU57x 原厂 USB 与 wireless dio_manager 实例仲裁、实车验证 |

## 3. 不可直接照搬的外部平台细节

参考项目是 MHI2Q QC / APQ8064；本工程是 AU57x 中国版 MU1005。即使同为 QNX 6.5 ARMv7，也不能直接假设：
- /dev/otg-cinemo 在 AU57x 存在或代表相同 driver；
- dio_manager.json 的 config key 10 / iap2.device 偏移相同；
- CDIOManager+0xE8、iAP2Connect() 调用地址相同；
- uap0、start-ap.sh、mdnsd.sh、pf.conf 路径和接口名相同；
- libbthook.so GOT hook 点、btstack ELF 布局、函数 ABI 相同；
- 参考项目的 libwcpstart.so、librgi_hook.so 或 HMI JAR 可直接装入 AU57x。

以上内容都必须以 AU57x 原厂 ELF、配置、脚本、服务注册表和运行证据重新确认。尤其不复制外部项目对 stock dio_manager / libairplay 的 hook 地址或 GOT patch。

## 4. 对本工程最重要的路线修正

此前 R105.021 已确认：ASI IapDeviceServices 没有原始 iAP2 byte/frame 通道，不再把它当 raw transport；inner_airplay 是适配器，不等于已注册到原厂 AirPlay event callback；AirPlayReceiverSessionSendCommand 是已识别的发送端 ABI，不能代替手机发来的 event/control 命令接收端；Wi-Fi AP 的历史连通不等于 AirPlay session / iAP2 inside AirPlay 已建立。

因此下一轮的首要任务不是再扩展 parser，而是复原 AU57x 的两条真实 owner：
A. AirPlay receiver event/control callback：谁注册、回调参数是什么、运行在哪个 queue/thread、如何将 iAPSendMessage 交给 inner_iap2；
B. stock iAP2 client：AU57x 在何种无线 session 时机启动、如何拿到 AirPlay channel 的 send/receive 两端、如何仲裁 USB 与 wireless。

## 5. 参考项目提供的具体调查清单（在 AU57x 上重新定位）

1. 从 AU57x dio_manager 的导入/调用点确认 AirPlay server/session 创建与 event client 注册。
2. 在 AU57x libairplay.so 及调用者中定位接收方向 event-channel reader、dispatch callback 和 reply queue；不能仅凭发送函数推定接收函数。
3. 在 AU57x dio_manager.json、启动脚本和相关服务配置中找 iAP2 device/path、Wi-Fi AP 配置、Bonjour 启动与 HMI offer 时机。
4. 在 AU57x btstack/BlueSDK 插件注册表中定位 RFCOMM server channel、CarPlay/iAP2 SDP 记录和连接回调；先做只读 ABI/静态复原。
5. 确认 AU57x 的原厂 iAP2 client 能否在 AirPlay session 中复用，是否需要由原厂 DIO/SI owner 排队启动；禁止假定 QC 的直接 job post 偏移。
6. 复原 AU57x 的 event-channel 单读者仲裁：stock receiver 和无线 iAP2 bridge 不能并发抢读；回复必须按 wire order 回到正确 owner。
7. 复原 Wi-Fi channel 与 mDNS/DNS-SD 注册的真实启动时序；不硬编码 uap0 或参考机地址。
8. 复原有线 CarPlay 优先级：USB session active 时不启动/不接管无线 receiver；USB 插入/拔出后的恢复必须遵守 AU57x 原厂 SI/DIO 生命周期。

## 6. 本轮结论与状态

本轮从公开参考项目获得的是一套在另一 MHI2Q/QC 机型上真实报告的端到端架构与测试证据。它支持 Bluetooth bootstrap + Wi-Fi AirPlay + iAP2 inside AirPlay session 的总体方向，但不是 AU57x 已验证实现，也不证明本工程当前代码已经接通。

当前 AU57x 已有：iAP2 packet parser/relay、AirPlay sender-side ABI adapter、DIO/SI lifecycle controls、QNX ARM build pipeline。
当前 AU57x 未有：真实 AirPlay event/control receive owner、无线 session 创建/触发、RFCOMM bootstrap 实车闭环、HMI offer 适配、有线/无线实车仲裁。

禁止把本报告作为刷机/安装依据。

installable=false / safe_to_flash=false / vehicle_verified=false / mfi_apple_verified=false。