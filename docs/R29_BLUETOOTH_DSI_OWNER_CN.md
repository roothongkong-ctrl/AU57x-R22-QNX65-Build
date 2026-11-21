# R29：MU1005 BluetoothSmartphoneIntegration 原厂高层模块静态复核

目标：2017 Audi A6L C7，MHI2_CN_AU57x_K1006_2 / MU1005。

已在用户保存的 R40 只读研究资料中找到高层原厂 ELF `bluetooth`（SHA-256 `7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272`）及 `btstack`（SHA-256 `51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566`），离线核对两个样本的完整 SHA 和7个静态 ELF 符号/字串标记，**7/7 符合**。这些原厂文件**没有上传到公共 GitHub**。

- `bluetooth` 中存在 `CBluetoothSmartphoneIntegration` 虚表、`BluetoothSmartphoneIntegrationServiceRegistration` 虚表、`interfaceID` 数据符号。相比此前只在 `dio_manager` 看到相应的 ASI proxy，定位到了可进一步研究的高层服务侧**对象实现候选**。
- 同一 `bluetooth` 中有 `iapEnabled`、`bluetooth.enableIap`、`SERVICETYPE_IAP2`。它们是配置/分支存在的静态证据，不能证明实际车机已开启 iAP2 或注册服务。
- `btstack` 中存在 RFCOMM tracing 标记；不能证明真实 RFCOMM callback 归属、通道编号和到 iAP2 设备的流动。

**尚未解决：** 真实服务运行注册、Bluetooth DSI → 原厂 mm-ipod/iAP2 跨进程路径、MFi 授权、SI 对 DIO 的合法启动事件、实车固件 SHA、Wi-Fi/AirPlay 会话及媒体交互。

复验：在自己的受控目录放原厂 `bluetooth` 和 `btstack` 后运行 `python3 tools/verify_mu1005_dsi_boundary.py --oem-dir /private/OEM --output /private/report.json`。绝对不要将这些原厂 ELF 上传公开仓库。该脚本仅作只读指纹和静态标记核验。

**结论：静态样本审计 PASS；无线 CarPlay、实际原厂 owner 和安装准备均为 NO_GO。**
