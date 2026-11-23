# R102：从成熟 GitHub 源码直接复用（不上车，不安装）

**结论：不用继续从零写协议。** 已经实际引入并编译上游 GNU GPLv3 源码 `luka-dev/mib2q-carplay-rgi/wireless/graft/eir.c`，其 Git blob 严格等于原仓库 `99f42933e7c6b98af11fb387a1e68727ffe31997`。它负责在 BlueSDK 的 240 字节 Bluetooth EIR 广播数据里安全加入 CarPlay UUID，避免多次添加同一 UUID。

## 已执行
1. 从 R101 FIX1 完整工程继承全部源码和已有恢复功能。
2. 引入上游 EIR 原始 C 文件（不修改、不跨车型移植原厂地址），保留 GPLv3 与来源信息。
3. 新增 `au102_eir_advertisement.[ch]` 只读入口：有线会话优先；只有原厂 btstack 身份、iAP2 RFCOMM 注册成功、SDP 发布成功这 3 项都得到运行时证明才允许追加无线广播 UUID。其他情况不修改任何数据。
4. 用上游原样源码 + AU57x 包装器运行主机 C 测试，覆盖 UUID 初次加入、重复去重、已有 UUID 字段扩展、畸形 AD 数据、EIR 容量、有线优先和未授权不修改。
5. 本工作 **未** 修改原厂 Bluetooth 进程、未找到真实 iAP2 注册线程，未安装无线 CarPlay；仅实现了可直接复用的广播构件。

## Github 可复用候选
- `congobg/MHI2Q_WiFi_CarPlay`：公开作者报告有实车运行，但当前公开仓库主要是文档，无完整可直接复制的程序源代码。
- `luka-dev/mib2q-carplay-rgi`：**正式首选源码库**。其 `wireless/graft/graft.c` 的真实 RFCOMM 数据队列、SDP/EIR、`wireless/daemon` 的 iAP2/MFi/Wi-Fi、`wireless/receiver_bridge` 的接收器接口，以及 `wireless/integration` 的生命周期可以成为后续集成来源。
- `joeyQuery/MHI2-wirelessCarPlay`：提供 MHI2/iAP2 DIO 原厂服务边界的逆向资料。
- `omonob/VW-MHI2-Wireless-Carplay-Maps`：虽同属 MHI2，但为大众/斯柯达 MU1102/1367/1440，包含不可直接跨固件装车的二进制，仅复用事务安装思想。

## 对应原车 MU1005 的差异与不可跳过验证
- 上游 MHI2Q 原 `RF_RegisterServerChannel` 目标地址约 `0x277B98`；你的 AU57x/MU1005 原厂反汇编目标是 `0x277A18`。**上游固定地址绝不能直接用在 MU1005**。
- 原厂 btstack/蓝牙服务回调线程、iAP2 通道参数及相应 SDP/设备身份仍须从 MU1005 原厂接口确认。不能借 HFP 的回调或 SIM 通道号作为 iAP2。
- MFi 不绕过；优先原车认证芯片；iAP2 空口协商需要真实 iPhone 完成首次最终验证。
- Wi-Fi/Bonjour/AirPlay/HMI 必须适配原车 DIO / SI 与 2.4GHz；有线优先。
- SD 安装和恢复由现有 R96 事务引擎作基础，待原厂启动入口适配后才能生成可装车测试候选包。

**按实际工作剩余 4 个关卡 + 1 次实车：** ①把上游 RFCOMM graft 适配到 MU1005 正确原厂回调/线程与广告发布；②复用 iAP2/MFi/Wi-Fi/AirPlay 上游协议和本车原厂接口；③验证有线优先、会话切换和一次性回滚；④合成 MU1005 原生 SD 安装候选并做整体 QNX 验证；⑤最后一次装车试验。其范围不等于只需把四个文件复制到 SD。

许可：上游 GNU GPLv3，保留原作者著作权、许可、对应源代码与修改记录。此包不包含任何上游或原厂的适用于其他固件的可刷机二进制。
