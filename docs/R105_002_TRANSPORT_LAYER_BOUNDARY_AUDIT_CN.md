# R105.002｜MU1005 双层 Transport 边界核对与下一步（离线整合）

日期：2026-11-26
基线：MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM Tegra T30
状态：STATIC_EVIDENCE_PARTIAL / NO_INSTALL / safe_to_flash=false

## 1. 本轮最重要的纠偏：两个 Bluetooth Transport 不是同一层

现有资料里出现两个容易混淆的对象：

### A. mm-ipod 的物理 Transport 插件层

原厂启动实例的形式为：

mm-ipod -d iap2,config=/etc/mm/iap2.cfg,... -t usbdevice,path=/dev/io-usb/otg_dev ...

它通过 ipod-transport-%s.so 动态装载 ipod_transport 模块。已完成的 USB 插件 ABI 研究确认：
- 插件名：ipod-transport-usbdevice.so，19,338 bytes，SHA256 e1743821b166fcd93992b45fc7064ad14763ee8a4445478e9aa36e696f2f6e95；
- ipod_module 版本 0x0200，模块对象指向 Transport 对象；
- 物理 Transport 表：+0x14 init、+0x18 link params、+0x1c type、+0x20 caps、+0x24 audio path、+0x28 send、+0x2c receive；
- USB 插件 ABI 的主机 dlopen/函数表测试和静态检查已有历史成果，但不代表蓝牙插件存在。

这层决定 mm-ipod 当前通过哪种物理 I/O 插件连接外设。不能把 USB 插件改名成 Bluetooth，也不能把 -t usbdevice 改成未经证明的名字；那会有损坏有线 CarPlay 的风险。

### B. ipod-drvr-iap2.so 内部的 iAP2 Component / Link Transport 层

MU1005 原厂 ipod-drvr-iap2.so（739,925 bytes，SHA256 42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf）的静态反汇编已确认：
- transport_get_link_params 通过 transport object +0x18 调用；
- transport_send_pkt 通过 transport object +0x28 调用；
- transport_receive 通过 transport object +0x2c 调用；
- transport object 由 link-private +0x40 提供；
- BluetoothTransportComponent、WiFiTransportComponent、TransportComponentIdentifier 等组件名存在于驱动字符串/配置路径中；
- bt_update_recv 是结构化 Bluetooth 状态/PPS 更新处理器，不能当成 RFCOMM 原始字节入口；bt_start_updates 是 iAP2 link message 路径，也不能当成 RFCOMM socket 注册函数。

这层属于 iAP2 驱动内部组件与 Link Engine 的抽象。它与 mm-ipod 外部 -t usbdevice 插件不是同一个接口。仅凭组件字符串或 iap2.cfg 中注释的 [bluetooth] 段，尚不能断言原厂蓝牙物理数据已接入，也不能断言打开该段就能完成 CarPlay。

## 2. 与 001 / R106 实车快照交叉核验

已有用户上传的 001 采集包已被历史报告解析；不要求再次上车或重复上传同一快照。

报告中可确认：
- QNX 6.5 / ARM Tegra T30；
- btstack、bluetooth、smartphone_integrator 等原厂进程在采样时运行；
- 蓝牙阶段没有 iAP2-over-RFCOMM 注册或完整 MFi 握手证据；
- Wi-Fi AP uap0 信道 11（2.4GHz）曾有客户端关联，但不证明无线 CarPlay；
- USB 插入后 mm-ipod -d iap2 ... -t usbdevice 启动，随后出现 carplay0、SI CARPLAYVERIFIED/STARTED、DIO/AirPlay _airplay._tcp 端口 5000、屏幕 READY 和音频播放；
- 001 的 OEM 文件复制汇总为 0，oem_files/ 为空；因此该快照没有带回 mm-ipod / transport plugin / 完整配置的原始字节。

**因此：实车快照证明有线原厂路径成功，也证明采样时 mm-ipod 使用 USB 物理 Transport；它既不证明无线 Transport 已经存在，也不否定 iAP2 驱动内部 BluetoothTransportComponent 的潜在实现。**

## 3. 已有资料状态（不重复向用户索取）

| 资料 | 当前可确认状态 | 下一步处理 |
|---|---|---|
| ipod-drvr-iap2.so | 原始 ELF 哈希和反汇编分析已存在 | 继续使用现有分析，不再重复索取 |
| ipod-transport-usbdevice.so | 用户先前已提供，19,338 bytes，哈希已记录 | 复用 USB ABI 作为接口基准 |
| iap2.cfg | 完整 70 版 dump 已记录在早期项目资料中；[bluetooth] 段原本注释 | 从既有 dump / app.img 提取原件并哈希固定 |
| mm-ipod | 早期 USB ABI 报告称使用过同版原厂 ELF，但当前可检索的 Library 元数据未确认独立原始文件条目 | 先检查已有 app.img / 固件包及历史工程归档，不先要求重新上传 |
| 001 实车快照 | R105/R106 已有结构化报告；001 OEM 原始文件目录为空 | 复用现有报告，不重复采集 |
| bluetooth / btstack / dio_manager / SI / AirPlay | 已有静态哈希、ELF 审计与调用图 | 只追缺失的跨进程 owner / transport producer |

## 4. 已经做过、不要再做的方向

1. 不再把 bt_update_recv 当作 RFCOMM 数据入口。
2. 不再在 link_get_transport_data / iAP2 packet parser 内盲找 Bluetooth socket 构造器；它是协议 packet/TLV 处理路径。
3. 不再重写 R103 的 Marker/SYN/LSP/ACK LinkEngine。
4. 不再将 HFP callback/thread 当作 iAP2 owner 证明。
5. 不再把 mm-ipod 的 USB ipod_transport ABI 与 iAP2 驱动内部 BluetoothTransportComponent 混为一个插件。
6. 不改 -t usbdevice、不替换原厂 USB transport、不修改车机启动配置，不做装车尝试。

## 5. 当前最有效的离线工作顺序

### Task A — 从已有 app.img / 固件归档精确取回 mm-ipod

只在用户已提供的原始固件和 app.img 中检索，不要求重新上传。固定 SHA256、ELF header、PT_LOAD、DT_NEEDED、符号、字符串和调用图。核实其动态加载 ipod-transport-%s.so 的参数来源、模块搜索路径、ipod_module 版本检查、初始化/退出和是否允许多个 transport 实例。

### Task B — 与已知 USB 插件逐槽对照

复用既有 USB ABI 结果，不再重新逆一遍。用 mm-ipod caller 的机器码确认它如何填 owner/context、如何调用 init/send/receive、错误时如何撤销。输出一份完整、带置信等级的 loader ABI JSON。

### Task C — 单独追 iAP2 驱动内部 BluetoothTransportComponent

在已有 ipod-drvr-iap2.so 中从配置字符串引用和 iap2_init 的 literal pool 开始，定位组件对象构造/工厂、TransportComponentIdentifier 映射、BluetoothTransportComponent 的方法表、调用者与 transport object 注入点。明确它是否由驱动内部蓝牙服务提供，还是需要外部 owner/IPC。不能从字符串推导函数行为。

### Task D — 最后才决定数据桥放在哪里

只有当 A/B/C 给出证据后，才选择：
- 若原厂 BluetoothTransportComponent 能被配置和真实 BlueSDK owner 激活：复用原厂实现，不新增 mm-ipod 蓝牙物理插件；
- 若组件存在但缺 RFCOMM 数据生产者：在经证明的原厂 owner/IPC 边界补最小可撤销桥；
- 若不存在可用组件：才考虑新增独立 ipod-transport-<name>.so，严格实现 ABI v2，并保留原厂 USB 实例不变。

以上为条件决策，不预判哪条路线成立。

## 6. 验收和安全

- 静态：原始 OEM SHA 固定、反汇编/重定位/调用图可复核；
- Host：loader ABI 布局、插件生命周期、收发/超时/撤销、断线和 USB 优先测试；
- QNX ARM32：目标 sysroot 编译与真实动态装载验证；
- Vehicle：真实配对 iPhone、Bluetooth iAP2/MFi、Wi-Fi、AirPlay 音视频和控制、有线抢占及恢复。

Host/静态通过不等于 QNX 或实车通过。任何真实 owner、插件 ABI、认证和回滚未确认时均保持 NO_INSTALL。

**本轮进展**：把两种不同层次的 Transport 边界厘清；确认已有 USB ABI 资料和 001/R106 实车报告可复用；下一步直接在既有 app.img/固件与原厂 ELF 上完成 mm-ipod loader + BluetoothTransportComponent 双线调用图，不再重复旧轮次的 HFP、PPS、packet parser 或 USB 插件逆向。
