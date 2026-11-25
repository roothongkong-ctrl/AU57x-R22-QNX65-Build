# MU1005 无线 CarPlay 原厂能力矩阵（静态 / 日志证据版）

目标：MHI2_CN_AU57x_K1006_2 / MU1005，QNX 6.5 ARMv7。仅为离线证据归档；NO INSTALL。

| 子系统 | 已证实 | 未证实 / 不可推断 | 当前研发动作 |
|---|---|---|---|
| Bluetooth 高层 | R106/001 快照中 btstack、bluetooth、smartphone_integrator 进程存在；R29 静态样本中 bluetooth 有 DSI 对象候选、iapEnabled / bluetooth.enableIap / SERVICETYPE_IAP2 字符串 | 进程存在和配置字符串不证明 iAP2 服务已注册；btstack 未见可直接移植的命名 iAP2 endpoint；真实 RFCOMM owner、回调序列、iAP2 SCN 未证明 | 以 R100 factory-first / reversible-graft gate 为准；只接受真实 owner、endpoint、注册参数与撤销/恢复证据 |
| RFCOMM / iAP2 | R101 静态审计中 HFP=7、SIM=4 经过同一注册函数；R103 将上游 iAP2 LinkEngine 与 8 字节 RFCOMM generation frame 桥接，具备主机 TCP lab | 7/4 不能推断为 iAP2 通道；R103 Rust QNX ARM 可执行尚未构建；真实 BlueSDK 回调/手机握手未验证 | 复验 R103 Rust/TCP CI；在 C/Rust 边界固定 generation、断链和 ACK；生产接入仍需 OEM iAP2 owner |
| MFi | R41 样本中 libairplay.so 导出 APSMFiPlatform_CopyCertificate / APSMFiPlatform_CreateSignature；R44 有 AA00–AA05 状态机和 ACK 语义 | 静态符号 + host mock 不等于 iPhone MFi 成功；没有真实 peer_auth_succeeded 证据 | 保留原厂 MFi API，不造证书/签名；验证 handle、ABI、allocator/free、owner thread |
| Wi-Fi 2.4GHz | R106/001 实车日志出现 uap0 active、MICRO_AP_EV_BSS_ACTIVE、MICRO_AP_EV_ID_STA_ASSOC；信道 11、10.173.189.1 | STA_ASSOC 不证明 iPhone CarPlay 会话，也不证明该客户端身份就是目标 iPhone | 以 2.4GHz 为基础研发路径；验证地址、路由、Bonjour 和 AirPlay 会话边界 |
| Wi-Fi 5GHz | uaputl 有 BAND 参数与 A-band 校验函数；Marvell 8787 固件/驱动样本含 A-band / fw_bands 相关逻辑；connectionmanager AP 模板默认频道 1/6/11 | 实际开机加载哪份固件、fw_bands 实际返回值、区域码、主板 PA/滤波器/射频通路未证实；不能承诺软件改配置即可启用 | 5GHz 保留条件分支，只有真实能力探测与硬件证据成立才启用；否则不尝试强制切换 |
| AirPlay OEM server | R86 从原厂 dio_manager 机器码定位 ServerSetDelegate 与 SessionSetDelegate 两级注册；回调结构 60/56 字节、21 个真实 GOT 槽；R106 有线路径出现 AirPlayReceiverSessionStart / READY | R86 是静态 ABI 证据，不证明无线 sessionCreated / media callbacks 真实触发；无线 Bonjour/AirPlay/iPhone 会话未证实 | 建立只读 delegate observer；不替换原厂回调、不改变返回值；按 session 身份关联生命周期 |
| SI / DIO | R106 证明有线 USB iAP2→dio_manager→AirPlay Receiver→carplay0 路径可用；R81–R85 已设计 SI/DIO 分进程 AF_UNIX 控制、状态快照和有线恢复协议 | 生产 peer identity、DIO restore、SI teardown、媒体与方向盘回调尚未定位为可安全调用的真实 ABI | 继续只读审计真实 SI/DIO owner 与 callback；SI 不持有 DIO 指针；wired takeover 必须双确认 |
| 音视频 / HMI | R86 回调表包含音频相关回调；R50 有视频、音频、MMI、steering 的 owner-gated contract | 无无线 CarPlay 实际视频/音频/触控/旋钮/方向盘数据流证据 | 先确认原厂 AirPlay decoder / DIO 媒体 sink 的真实回调来源；无 owner 时拒绝报告 MEDIA_ACTIVE |

## 证据出处

- 仓库：docs/R29_BLUETOOTH_DSI_OWNER_CN.md、docs/R38_MU1005_BLUETOOTH_CALLBACK_CN.md、docs/R41_STOCK_MFI_INTEGRATION_CN.md、docs/R43_IAP2_LINK_UPSTREAM_PORT_CN.md、docs/R44_FULL_LINK_TO_STOCK_MFI_CN.md、docs/R50_CLOUD_04_TO_11_CN.md、docs/R81_SI_DIO_SEPARATE_PROCESS_IPC_CN.md 至 docs/R85_SI_DIO_WIRED_RESTORE_CN.md。
- 私有用户资料：R86_ORIGINAL_OEM_DELEGATE_ABI_CN.md；AU57x_MU1005_WiFi_5GHz_三文件复核报告_20261122.md；R100_FACTORY_TO_GRAFT_CN.md；R103_UPSTREAM_IAP2_LINK_BRIDGE_CN.md；AU57x_R106_001_实车日志分析_20261123.md。

## 结论

已证实存在 MU1005 原厂蓝牙栈、iAP2 相关组件、原厂 MFi API、2.4GHz AP 运行能力和可复用的有线 AirPlay/DIO 链路；尚未证实蓝牙 iAP2 真实注册、iPhone MFi、无线 AirPlay 会话及无线音视频。5GHz 有软件处理代码，但物理射频能力仍未知。当前不应生成或安装无线 CarPlay 修改包。

状态：RESEARCH_ONLY / NO_INSTALL / safe_to_flash=false / vehicle_verified=false。