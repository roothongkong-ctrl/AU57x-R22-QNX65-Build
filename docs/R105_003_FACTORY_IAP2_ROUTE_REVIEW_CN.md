# R105.003｜MU1005 原厂 iAP2 路线复核与优先级调整

日期：2026-11-26
基线：MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
状态：STATIC_EVIDENCE_ONLY / NO_INSTALL / safe_to_flash=false

## 0. 本轮结论

重新对照 R88、R99、R100、R104、R106 及用户历史交接文件后，当前最优先的不是新增 mm-ipod 蓝牙物理插件，而是：

1. 复核原厂 `ipod-drvr-iap2.so` 的 BluetoothTransportComponent 选择/构造机制；
2. 查明原厂 `bluetooth` 配置的真实路径、JSON schema 和 enableIap 的运行时消费者；
3. 证明启用 iAP2 后 BlueSDK RFCOMM owner、iAP2 driver transport 和 `/dev/ipod0` 是否形成真实数据链；
4. 在此之前，不把 R99 双配置暂存结果当成可安装方案。

mm-ipod 的 `-t usbdevice` 物理插件属于已证实有线链路的一部分；现有资料没有证明它就是蓝牙 iAP2 的必经接口。R105.002 对 mm-ipod loader 的双线审计保留为辅助核对，但不再作为当前主攻路线。

## 1. 已有证据重整

### 1.1 MU1005 原厂样本指纹

| 样本 | SHA256（公开工程只保留指纹，不含原厂字节） | 证据边界 |
|---|---|---|
| `bluetooth` | `7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272` | 含 `bluetooth.enableIap`、`iapEnabled`、`SERVICETYPE_IAP2` 字符串；未证明配置路径、实际值和服务注册 |
| `btstack` | `51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566` | 含 RFCOMM、HFP/SIM callback 和 BtJobQuerySdp/BtJobStartService 相关符号；未证明 iAP2 SCN、回调 owner 和 iAP2 数据路径 |
| `ipod-drvr-iap2.so` | `42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf` | 含 Bluetooth/WiFi transport component 与 link transport 接口字符串；尚未完成 component factory / ID 映射和真实 producer 追踪 |
| `smartphone_integrator` | `2c61e7a17d5c77df2f97023284b948b66dc4052010acad74be9bfa63b339de0b` | 原厂 CarPlay/SI 样本；不代表蓝牙 iAP2 已启动 |
| `dio_manager` | `e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c` | 有线 iAP2 client 与 AirPlay/DIO 入口；R106 证明 USB 会话成功 |

### 1.2 既有 R88：不要重复做的事

R88 已完成以下离线静态审计：
- 原厂 btstack 与 bluetooth ELF SHA 校验；
- btstack 中 RFCOMM / SDP 相关静态符号和回调标记核对；
- 原厂 bluetooth 中 IAP enable / service enum 标记核对；
- R45/R39 RFCOMM server-first event → AuRfIngress → R87 状态机的 host 适配器已存在。

R88 没有证明真实蓝牙 callback owner 可安全 attach，也没有证明 iPhone iAP2/MFi 成功。它是 phone-initiated server-first 候选，不等于已经有 CarPlay 会话。

### 1.3 既有 R99：配置工具的边界

R99 工具当前源代码做的是离线、可回滚的双配置暂存：
- 对 bluetooth JSON 的唯一 `enableIap` 布尔键置 true；
- 对 `iap2.cfg` 中唯一完整的注释版 Bluetooth section，解除 `enable/id/name/connectstatus/mac` 五个字段注释；
- 原文件保留、哈希校验、独立二次复核，明确 `installable=false`。

但它不做以下事情：
- 不证明实际车机加载的 bluetooth JSON 路径/schema；
- 不证明 `enableIap=true` 的真实运行时消费者；
- 不证明 `iap2.cfg` 的 Bluetooth section 会被实例化；
- 不证明其中的 transport ID/name/MAC 与 MU1005 当前 BT identity 匹配；
- 不证明 BT RFCOMM 原始数据进入 iAP2 driver；
- 不证明 MFi 或无线 AirPlay。

特别是 `mac` 字段：虽然历史 dump 中有值，但在确认它代表本机适配器地址、目标手机地址还是 transport peer identity 之前，不能把它解释为可直接沿用的常量，也不能在用户设备间复用。

R99 旧版 demo audit 曾显示“新增空 Bluetooth section”的旧行为；当前 R99 源码已修复为只激活完整原模板。旧 demo JSON 不应被当作当前生成器的最新验收结果，后续 CI 必须重新生成和验证。

### 1.4 R100 的重要门禁

R100 静态审计给出的状态仍是：
- `route_eligible_for_auto_activation=false`
- `stock_btstack_named_iap_endpoint_observed=false`
- `stock_btstack_named_iap_services_observed=false`
- `factory_iap2_runtime_proven=false`
- 缺失项包括真实 supervisor launch config、BlueSDK owner callback、RFCOMM iAP2 profile、可信回滚控制和真实 MFi/AirPlay handoff。

这些状态不能被 R99 的配置暂存工具覆盖。R99 PASS 只代表离线变更受控，不代表 R100 的运行门禁已通过。

### 1.5 R104 的 iAP2 driver 反汇编事实

已知函数：
- `iap2_init` @ 0x0ea6c
- `init_features` @ 0x0e2cc
- `bt_update_recv` @ 0x0505c
- `bt_start_updates` @ 0x0553c
- `bt_stop_updates` @ 0x054a8
- `bt_send_info` @ 0x056b0
- `transport_get_link_params` @ 0x21144
- `transport_send_pkt` @ 0x2115c
- `transport_recv_pkt` @ 0x2121c

已确认 `bt_update_recv` 是结构化状态/TLV → PPS 的更新处理路径；`bt_start_updates` 发送 iAP2 Link 消息 0x4e03。两者都不能冒充 RFCOMM 原始数据入口。

## 2. 关键纠偏：两个 transport 层分开处理

- `mm-ipod -t usbdevice`：外部物理设备插件，R106 有线 CarPlay 实车日志已证实使用该路径。
- `ipod-drvr-iap2.so` 的 `BluetoothTransportComponent` / `WiFiTransportComponent`：iAP2 driver 内部 link transport 抽象；由哪个 factory/identifier 实例化、如何绑定真实 BT owner，仍待反汇编确认。

不得将 `ipod-transport-usbdevice.so` 改名或替换为蓝牙插件，也不得将 `-t usbdevice` 改成未经证明的参数。

## 3. 下一步只做这些有增量的工作

### Task A — 配置消费者和真实路径（优先）

对已有 app.img / 固件归档和私有原厂文件做离线索引，不要求用户重复上传：
1. 找出 `bluetooth` 实际读取的配置文件路径及配置 schema；
2. 对 `bluetooth.enableIap`、`iapEnabled`、`SERVICETYPE_IAP2` 做代码交叉引用，确认是配置读取、服务注册条件还是仅日志/枚举；
3. 对照 connectivity launcher 的真实启动配置，确定 bluetooth 服务实例的启动顺序和依赖；
4. 输出“配置路径→解析键→分支→服务注册”的引用链。只有字符串没有 caller 的项目必须标记为未证实。

### Task B — iAP2 driver component factory / transport ID map（核心）

对精确哈希的 `ipod-drvr-iap2.so`：
1. 从 `iap2_init` 的字符串 literal pool 和配置读取函数调用开始，定位 INI parser、section/name/id 读取；
2. 追踪 `TransportComponentIdentifier`、`NativeTransportComponentIdentifier`、`TransportComponentName` 和 `BluetoothTransportComponentIdentifier` 的交叉引用；
3. 定位 component factory/constructor、vtable 和 identifier→class 映射；
4. 还原 `transport_get_link_params` / `transport_send_pkt` / `transport_recv_pkt` 的间接调用对象字段，确认对象由谁创建；
5. 对 `BluetoothTransportComponent` 追踪到实际 producer：BlueSDK callback、ASI/IPC、PPS 或其他数据源。未找到 producer 时不能声称配置已足够。

### Task C — BT owner / RFCOMM / iAP2 对接

复用 R88 的 host server-first adapter 和 R103 LinkEngine，不重写协议层：
1. 明确 MU1005 RFCOMM 事件 owner、线程、回调参数、连接代际、SDP 服务记录；
2. 证明 iAP2 的实际 profile UUID/SCN/认证要求；HFP=7、SIM=4 不能用作 iAP2；
3. 将 BT callback 到 iAP2 transport 的数据路径按实际方向画图，确认发送与接收双向；
4. 明确连接失败重试、断线清理、手机切换、USB 有线抢占的状态边界。

### Task D — 配置候选的安全复核（不生成安装包）

在 Task A/B 完成前，R99 的 staged 配置只作为 review artifact。特别核对：
- Bluetooth section 是 transport 定义还是额外配置 section；
- transport ID 1111 与当前活动 Lightning ID 1234 的作用域和冲突规则；
- MAC 字段语义及其与设备/手机地址的对应；
- 原厂 parser 是否允许同时声明多个 transport，及默认优先级；
- `enable=yes` 与 bluetooth JSON 的 `enableIap=true` 是否需要额外服务端配置或 supervisor restart。

不得猜测字段含义、生成新的 ID/MAC 或修改原厂有线配置。

## 4. 交付门禁

| 阶段 | 通过条件 | 未通过时 |
|---|---|---|
| 静态 driver | factory、ID map、vtable、send/receive caller 都有反汇编证据 | 保持 STATIC_PARTIAL |
| 配置路径 | 原厂配置路径、schema、键消费者和 supervisor 启动顺序有 caller 证据 | 不修改配置 |
| BT owner | 真实 BlueSDK owner、RFCOMM profile/SCN、回调线程和撤销路径可复核 | 不生产绑定 |
| host integration | R88/R103 adapter 双向、断线、重试、USB 优先测试通过 | 仅保留 host |
| QNX ARM | 目标 ABI/依赖/装载/退出码验证 | 不部署 |
| iPhone/MFi/AirPlay | 真实 iPhone 合法认证、Wi-Fi、音视频与控制完整会话 | 不宣称成功 |
| installer | 备份、校验、回滚、50/70 硬件指纹和安装门禁闭环 | `NO_INSTALL` |

## 5. 本轮明确不做

- 不重做 R103 LinkEngine、R88 RFCOMM host adapter、R86 DIO delegate ABI；
- 不重复 HFP/SIM channel 逆向；
- 不重做 USB plugin ABI；
- 不改原厂 btstack / bluetooth / iap2 driver；
- 不启用 R99 staged config，不改车机、不上车；
- 不把旧 R99 demo audit 当成当前源码行为。

## 6. 结论

原厂软件中存在 iAP2 相关配置和 Bluetooth transport 组件代码，这是值得继续追的路线；但 R100 的真实 owner/服务注册门禁仍未通过。最关键的缺口已从“缺一个蓝牙插件”收敛为“缺配置消费者与 driver component factory/真实 BlueSDK producer 的静态调用链”。先解决这两条，才能决定原厂配置直启、最小适配，还是回退到受控 RFCOMM graft。

状态：`RESEARCH_ONLY / NO_INSTALL / safe_to_flash=false / vehicle_verified=false`。
