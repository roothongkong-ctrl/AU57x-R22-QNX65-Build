MU1005 SI registration adapter (NO INSTALL). Based on private target SI SHA256 2c61e7a17d5c77df2f97023284b948b66dc4052010acad74be9bfa63b339de0b. Target addUniqueID at candidate 0x16ff4c saves r2 at entry, reads it at 0x1701a8 and passes it as assignment destination at 0x170534. Native discovery caller 0x17c75c passes r1 key and r2=info+24; 0x17ca98 updates with the resulting info. Update copies three strings from info+24/+64/+104 to record+32/+72/+112. Constructor allocation is 168 bytes. The adapter accepts an already constructed native info and distinct native key from a verified owner; it does not construct native metadata, establish ownership or attach to stock processes. Registration uncertainty is terminal until native state is reconciled; callback side effects before an error are not undone here. No real MFi, original process call, wireless media, or vehicle readiness is claimed.

## R105.008｜传输层路线复核

明确区分 mm-ipod 外层物理 Transport、ipod-drvr-iap2.so 内部 TransportComponent、btstack RFCOMM owner 三层；保留原厂 usbdevice，不把内部 BluetoothTransportComponent 字符串当作已存在的外层蓝牙插件。R103 上游 iAP2 LinkEngine 作为协议层复用，R98/R45 作为原厂接入层候选。当前分支增加 HOST-ONLY Rust/C 回归门禁。详见 [R105.008](R105_008_ROUTE_LAYER_RECONCILIATION_CN.md)。仍不可安装。


## R105.009–R105.010｜原厂 owner 非阻塞与经典 MHI2 donor 对照

- [R105.009](R105_009_OWNER_NONBLOCKING_BOUNDARY_CN.md)：封闭阻塞式 native bind 入口，只允许 worker 预建 fd 后由 owner 接管。
- [R105.010](R105_010_CLASSIC_MHI2_DONOR_COMPARISON_CN.md)：对照 classic MHI2 MU1440、MHI2Q MU1316、MH2p，优先核查本车原厂 Type-3 transport 是否能复用 stock iAP2/MFi；R103 保留备用。
