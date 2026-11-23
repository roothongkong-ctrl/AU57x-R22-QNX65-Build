# MU1005 无线 CarPlay：多路线突破与停止条件（2026-11-24）
目标：2017 Audi A6L C7 / MHI2_CN_AU57x_K1006_2 / MU1005 / Tegra QNX 6.5。
**唯一车上成功标准**：iPhone 实际蓝牙 iAP2 → 原厂 MFi 签名通过 → Wi‑Fi AP/STA + AirPlay → 视频、音频、输入全通；且有线 CarPlay 保留、支持卸载与恢复。
此文件**仅为现有工程的具体路线和证据清单**，不是安装说明。

## 首选 A：原厂 Bluetooth / iAP 入口（尽量 0 btstack 注入）
**已知实证**（用户 MU1005 ELF 离线审计 R88、R100）：
- `bluetooth` 字符串存在 `bluetooth.enableIap`, `SERVICETYPE_IAP2`, `iapEnabled`。
- `ipod-drvr-iap2.so` 有 `BluetoothTransportComponent`, `BluetoothiAP2Link`；原厂 `libairplay.so` 的 `APSMFiPlatform_CopyCertificate`、`APSMFiPlatform_CreateSignature` 已提取导出和签名长度信息（R41）。
- `btstack` 有 RFCOMM/SDP、HFP/SIM 注册调用，**但没有**在文件明文中找到 MU0678 的 `/dev/iapDevice` 和 `CIapBTChannel`；不能推导为不存在，只是**禁止照搬 MU0678 的设备路径和地址**。
**具体突破目标**：继续静态回溯 `bluetooth.enableIap` 的 producer/consumer、`ipod-drvr-iap2` 的蓝牙传输工厂、app.img 中真实的连接管理器配置；只在能够确认单一原厂权限所有者和 endpoint 时把原厂 RF 字节流交给已存在的 iAP2/CarPlay 栈。
**停门槛**：不能在真实 MU1005 runtime 中找到 endpoint/owner 的可信来源时，绝不假称“原厂隐藏无线开关已打开”。

## 并行 B：单进程原厂 btstack graft（保留有线）
**已知基础**：R88 原厂 `btstack` SHA-256 = `51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566`；R92 `LD_PRELOAD` 和 `pthread_create` 的四个线程调用点是静态核验的，**不是** BlueSDK owner 验证。
R101 原厂 HFP/SIM register 第三参数有 7/4 两种取值，不能把 donor 参数 0 当 MU1005 原厂 iAP2 通道号。
**可以复用**：`project/source/legacy_mu1005/bluetooth/inbound.c` 的 SDK 对象、SDP 发布；`r113/src/mu1005_native_gate_adapter.c`；开源 iAP2 link、`R41–R44` stock MFi。
**停门槛**：原生线程入口、第三注册参数、唯一 SDP/owner、回滚、USB 优先缺一即 `NO_INSTALL`。现阶段未到装车条件。

## 并行 C：旁路 CarPlay 会话服务（不修改原厂蓝牙 ELF）
**构型**：独立 QNX 用户态服务 + 现有蓝牙控制层提供的**真实**授权 RFCOMM endpoint（必须先验证这个接口）；开源 GPL3 EntryNav2 `iap2_link` 仅做链路分包/重传；保留本工程 R41–R44 对原厂现有 libairplay MFi 证书/签名，以及 R34 AirPlay server 包装，不自签不绕过认证。
**已做到**：Host 上真实 `inbound_send` → iAP2 C API、分包和 TX 完成顺序通过。
**尚未做到**：独立服务如何获得原厂 SDK RFCOMM endpoint、是否能在不改变 btstack 的情况下连接；原厂 libairplay 的 live ownership。未证实前不称“可不注入直接连接”。

## 低优先 D：复用原厂有线 iAP2 / USB 车机接收器
通过一个**已验证 ABI**的 transport adapter 将原有 USB 下层替换成 authenticated Wi‑Fi/AirPlay 会话，保留 factory MFi/AV视频。
**风险**：已有日志 `mm-ipod ... -t usbdevice` 与 `/dev/ipod0` 是 USB 实例，MU0678 的 QNX 消息结构不能直接套到 MU1005，更不能只用符号重命名当做无线。只有原厂 DIO/SI 真正接受无线 source session 的证据时才实现；否则不做破坏性改动。

## 务实后备 E：外置无线 CarPlay 转接器（明确非纯软件）
iPhone → 无线 → 转接盒 → 原车有线 CarPlay USB。保留已验证的有线及仪表投屏，不用修改 btstack/MFi；最可能迅速达到日常无线体验，但**不符合当前用户的“纯软件无线”首选目标**，不擅自替用户选购。部分转接盒与投屏 App 的兼容性需要单独确认。

## 频段关键纠偏
Apple 官方 WWDC17：无线 CarPlay **允许 2.4GHz**，5GHz 是强烈推荐，2.4 同信道活跃会话对蓝牙并行连接有额外限制。不能因为 R106 只看到 `uap0` 2.4 GHz 就宣布软件方案不可行。
88W8787 *芯片* 支持 802.11 a/b/g/n 双频（Marvell 产品资料），但市售某些 88W8787 *整机模组* 只有 2.4GHz 前端/天线。故不能根据固件驱动名字推断本车必有 5GHz。先按 2.4 GHz 研发；5GHz 若经原厂 RF/PHY 证据确认则支持优先。
Roland8810 的 MH2p `smartphone_integrator` 有 5GHz 运行时门禁，但它不是本车 MU1005；不要拿其他平台的开关判定本机。独立接收器若真需要绕过 stock SI，需要原生 AirPlay/DIO ownership 证明，不能靠“关掉门禁”假定功能已正常。

## 只做下一轮真正的静态工作，不重复 R106
1. 优先逆向已有 `bluetooth.enableIap` 的注册源与 consumer、`ipod-drvr-iap2.so` transport factory；现有 R88/R100 证据已可用于决定优先级，但不足以决定具体调用 ABI。
2. QNX ARM 编译修复后，使用独立第三方 core 作为唯一 Link 引擎；既有 R41-R44 原厂 MFi 作为唯一认证逻辑；禁止同时驱动两份 iAP2 session。
3. 如果 A 无法验证 owner/endpoint，转向 B/C 的源码级真实 ABI；不存在已验证 provider 只能明确 `NO_GO`，不能继续堆假测试。
4. 有可回滚/保留有线的原厂真实入口后才制作 SD 安装包；不重复 R106 采集，不要求打开 SSH。

## 参考（资料与用户已有源码）
- Apple WWDC17: https://developer.apple.com/videos/play/wwdc2017/717/
- Marvell 88W8787 chip: https://www.yumpu.com/en/document/view/3505216/marvell-avastar-88w8787-soc-product-brief
- joeyQuery MU0678: https://github.com/joeyQuery/MHI2-wirelessCarPlay/blob/main/traces/TRACE-019-exhaustive-five-track-closure.md
- luka-dev MHI2Q: https://github.com/luka-dev/mib2q-carplay-rgi/blob/main/docs/wireless-carplay/README.md
- EntryNav2 GPL3 iAP2: https://github.com/oguzkarcan/EntryNav2-CarPlay
- Our previous R41/R88/R100/R101 audit and source under project/source and docs
安全状态：`installable=false`; `safe_to_flash=false`; `vehicle_wireless_verified=false`.


## 补充候选 F：同为 MHI2 的大众/Škoda 无线实验包（2026-11-17）
新发现：omonob/VW-MHI2-Wireless-Carplay-Maps V3.2：
https://github.com/omonob/VW-MHI2-Wireless-Carplay-Maps/releases/tag/v3.2
其 README 说明 MU1102、MU1367 和 Škoda MU1440（特定仪表）为实验范围；MU1367 有 SD 注册/功能安装的用户反馈，但不等于全部无线音视频已完成本版实车验收；**没有支持 MU1005/AU57x 的依据**。
与 AU57x 最值得研究的不是成品 ZIP，而是：
- 保留原厂连接管理二进制，仅匹配固件后接入无线引导；USB 有线可保留
- btstack 的进程级 LD_PRELOAD 包装；不要把环境继承给其它进程
- 取得本机 SSID/密码，明确手机配对蓝牙和加入车机 AP 是两个条件
- 投屏/接收器/原厂 HMI/音视频会话权属分开；SD 软件更新注册与真正功能安装分开
- 事务备份、校验、失败回滚，未知 OEM SHA256 明确拒绝
**不能做**：将他们 MU1102/1367/1440 的 RF 固件映像、连接程序、签名参数、仪表分辨率、安装脚本直接用于 K1006_2/MU1005。
V3.2 release 只公开独立安装包和文档，没有单独可重复构建的全部源码，不能据此将载荷冒充开源 MU1005 适配或“已实测”。
额外参考：
https://github.com/omonob/VW-MHI2-Wireless-Carplay-Maps/blob/main/docs/WIRELESS_DEVELOPMENT.zh-CN.md
