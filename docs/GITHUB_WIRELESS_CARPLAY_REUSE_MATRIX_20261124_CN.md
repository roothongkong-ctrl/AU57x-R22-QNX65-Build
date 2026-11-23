# GitHub 复用决策 / 2026-11-24 (AU57x MU1005, QNX 6.5, Tegra)
| 项目 | 原生平台 | 可以直接借用 | 当前不能直接用 |
| --- | --- | --- | --- |
| joeyQuery/MHI2-wirelessCarPlay | NVIDIA MHI2 MU0678/QNX6.5 | 反编译证据和原厂 IapDevice / CIapBTChannel / enableIap 追踪方向 | MU0678 地址、字节布局、设备名未在 MU1005 复核；研究仓库未提供明确 LICENSE，不复制代码 |
| oguzkarcan/EntryNav2-CarPlay | BMW EntryNav2 Linux ARM | **已经实际导入本仓库** GPL3 iAP2 link/wire C++11 pure codecs | 原厂车机、DIO、HMI、Wi-Fi/BT 驱动不同，不能搬固件 |
| lvalen91/carplayd / CPC200-CCPA_resources | USB NCM/CCPA Linux | 真实无线 iAP2 消息顺序和鉴权/交接参考，MIT? carplayd 为 Unlicense | 不直接把 Linux BlueZ 改称 QNX OEM btstack |
| luka-dev/mib2q-carplay-rgi | Qualcomm MHI2Q ARM QNX6.5 | iAP2/MFi Rust daemon 状态机、QNX I2C、Wi-Fi/airplay preflight 源码参考；之前已用于架构 | btstack 机器码补丁和固定地址是 MHI2Q，不适用 AU57x |
| congobg/MHI2Q_WiFi_CarPlay | Qualcomm MHI2Q ARM QNX6.5 | 成功车型的 MFi+AirPlay 启动/重试经验 | Qualcomm ABI/HMI/接口不同，不可直接复制二进制 |
| f-io/LIVI / UnmetDecade/AllPlay | Linux/macOS CarPlay receiver | 开源 iAP2, AirPlay, MFi protocols concept | Linux 特有蓝牙和多媒体后端不能直接运行 QNX |
原厂复用优先新路径：先审 MU1005 btstack/iap/IapDeviceServices 是否具备**原生** RFCOMM iAP2 服务与动态节点（MU0678 存在，但 MU1005 未证实）；若存在，复用原厂并**避免另注入第二套 RFCOMM**。R106 只显示 btstack 进程、USB /dev/ipod0、mm-ipod；没有此节点的只读清单，不能断言 MU1005 无该功能。
重要纠偏：MU0678 文档的 iap2.cfg [bluetooth] 为连接状态配置，注释并不能证明禁用蓝牙数据传输；不把修改此段作为 MU1005 上线条件。
补充：MU0678 libiap2client <-> ipod-drvr-iap2 的 20-byte 0x9999 QNX MsgSend ABI 属于 USB/iPod 驱动。MU0678 btstack IapDevice 只是原生 BT byte stream; **禁止直接把 DIO 的 /dev/ipod0 换成 /dev/iapDevice** 而不证实 ABI。
详细来源:
https://github.com/joeyQuery/MHI2-wirelessCarPlay/blob/main/traces/TRACE-019-exhaustive-five-track-closure.md
https://github.com/oguzkarcan/EntryNav2-CarPlay
https://github.com/lvalen91/CPC200-CCPA_resources/blob/main/documentation/02_Protocol_Reference/wireless_carplay.md
https://github.com/luka-dev/mib2q-carplay-rgi/blob/main/docs/wireless-carplay/README.md
