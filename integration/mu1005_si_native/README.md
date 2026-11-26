MU1005 SI registration adapter (NO INSTALL). Based on private target SI SHA256 2c61e7a17d5c77df2f97023284b948b66dc4052010acad74be9bfa63b339de0b. Target addUniqueID at candidate 0x16ff4c saves r2 at entry, reads it at 0x1701a8 and passes it as assignment destination at 0x170534. Native discovery caller 0x17c75c passes r1 key and r2=info+24; 0x17ca98 updates with the resulting info. Update copies three strings from info+24/+64/+104 to record+32/+72/+112. Constructor allocation is 168 bytes. The adapter accepts an already constructed native info and distinct native key from a verified owner; it does not construct native metadata, establish ownership or attach to stock processes. Registration uncertainty is terminal until native state is reconciled; callback side effects before an error are not undone here. No real MFi, original process call, wireless media, or vehicle readiness is claimed.

## R105.008｜传输层路线复核

明确区分 mm-ipod 外层物理 Transport、ipod-drvr-iap2.so 内部 TransportComponent、btstack RFCOMM owner 三层；保留原厂 usbdevice，不把内部 BluetoothTransportComponent 字符串当作已存在的外层蓝牙插件。R103 上游 iAP2 LinkEngine 作为协议层复用，R98/R45 作为原厂接入层候选。当前分支增加 HOST-ONLY Rust/C 回归门禁。详见 [R105.008](R105_008_ROUTE_LAYER_RECONCILIATION_CN.md)。仍不可安装。


## R105.009–R105.010｜原厂 owner 非阻塞与经典 MHI2 donor 对照

- [R105.009](R105_009_OWNER_NONBLOCKING_BOUNDARY_CN.md)：封闭阻塞式 native bind 入口，只允许 worker 预建 fd 后由 owner 接管。
- [R105.010](R105_010_CLASSIC_MHI2_DONOR_COMPARISON_CN.md)：对照 classic MHI2 MU1440、MHI2Q MU1316、MH2p，优先核查本车原厂 Type-3 transport 是否能复用 stock iAP2/MFi；R103 保留备用。


## 22 步执行进度：第 01 步

- [R105.011：mm-ipod 外层 Transport 插件 ABI 静态解析](R105_011_STEP01_MM_IPOD_PLUGIN_ABI_CN.md)
- 已完成 loader 结构与七个 callback 槽位映射；新增 host C layout gate。
- 函数签名、生命周期、运行时实际加载插件尚未证明；不安装。


## 22 步执行进度：第 02 步

- [R105.012：原厂 ipod-drvr-iap2.so 内部结构静态分析](R105_012_STEP02_IPOD_IAP2_DRIVER_INTERNAL_MAP_CN.md)
- [R105 Step 02 静态审计 JSON](../../docs/R105_STEP02_DRIVER_STATIC_AUDIT.json)
- 已完成原厂 ELF 指纹、外层 Transport send/receive 分发、蓝牙状态更新和 Wi-Fi 配置消息静态映射。
- RFCOMM 原始字节入口、BT/Wi-Fi 组件构造及 vtable 尚未证明。


## 22 步执行进度：第 03 步

- [R105.013：mm-ipod 动态插件加载与原厂 USB Transport ABI](R105_013_STEP03_MM_IPOD_PLUGIN_LOADER_AND_STOCK_USB_ABI_CN.md)
- [R105 Step 03 静态审计 JSON](../../docs/R105_STEP03_MM_IPOD_PLUGIN_LOADER_AUDIT.json)
- 已静态确认 mm-ipod 动态加载器、ipod_module 接口和 USB Device/Host 共同回调布局。
- 蓝牙 RFCOMM 原始字节到 iAP2 transport 的桥接仍未证明；本次交接包内未找到蓝牙/Wi-Fi ipod transport 模块。


## 22 步执行进度：第 04 步

- [R105.014：MU1005 原厂 Bluetooth / btstack iAP2 owner 边界审计](R105_014_STEP04_BTSTACK_IAP2_OWNER_BOUNDARY_CN.md)
- [R105 Step 04 静态审计 JSON](../../docs/R105_STEP04_BTSTACK_IAP2_OWNER_STATIC_AUDIT.json)
- 已确认 Bluetooth app 的 iAP 配置标记、SmartphoneIntegration DSI 注册类、btstack 通用 SDP/RFCOMM 基础设施和 ASI iAP RPC proxy 边界。
- iAP2 专属 RFCOMM owner、SCN、callback ABI 和运行时绑定仍未证明。
