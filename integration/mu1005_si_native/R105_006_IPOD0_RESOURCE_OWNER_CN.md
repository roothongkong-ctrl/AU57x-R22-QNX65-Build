# R105.006｜/dev/ipod0 Resource Manager 所属链静态核对

日期：2026-11-26
目标：MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
分支：`mu1005-si-cprx-real-review`
状态：OEM_STATIC_REVIEW_ONLY / NO_INSTALL / vehicle_verified=false

## 结论先行

目前最可信的原厂结构是：

`mm-ipod` 进程/宿主
→ 动态加载 `ipod-drvr-iap2.so`（iAP2 driver module）
→ 该模块通过 QNX Resource Manager 相关接口提供 iAP2 设备节点服务
→ DIO 从配置读取 `/dev/ipod0` 并通过 `libiap2client.so.1` 连接。

因此，**/dev/ipod0 的资源管理实现最可能属于 mm-ipod 所加载的 ipod-drvr-iap2.so 模块**，而不是 DIO、btstack 或 ASI IapDeviceServices。由于当前没有对原厂 driver 的 `ipod_resmgr_mount` 调用点及实际 mount path 做反汇编闭环，也没有车辆运行时 pidin/mount 证据，本报告不把具体 mount 路径和进程 PID 写成已实测事实。

## 1. 证据链

### 1.1 mm-ipod 是模块宿主

R31 原厂 `mm-ipod` 审计固定 SHA256：
`4b87cd2d41ddcee2f38800fe8afe8b4b340e4f23e1d86c7e97af303522aa15d09`。

其静态标记包含：
- `device_transport_init`
- `dlopen` / `dlsym`
- `ipod-transport-%s.so`
- `ipod-drvr-%s.so`
- `/dev/ipod`

这表明 mm-ipod 有动态驱动模块与物理 Transport 模块两级加载机制。仅凭字符串不证明运行时选中的具体模块；但 R40/R44 对 USB 插件和 driver 模块的进一步 ABI 机器码核验已把加载关系具体化。

### 1.2 driver module 身份与 Resource Manager 证据

MU1005 `ipod-drvr-iap2.so`：
- 大小：739,925 bytes
- SHA256：`42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf`
- ARM32 little-endian QNX ELF32 DYN
- 导出 `ipod_module`
- 导入 `ipod_resmgr_mount`
- 同时导入 `iofunc_attr_init`、`iofunc_open_default`、`iofunc_read_verify`、`iofunc_close_ocb_default`、`iofunc_notify`、`MsgReply`、`MsgReplyv`、`MsgWrite`、`devctl`、`open/read/close` 等 QNX 资源管理器常见接口。

这些导入组合是该模块实现 QNX 设备服务/消息处理的强证据。单个 import 不能证明具体哪个函数调用了它，也不能单独证明注册路径一定是 `/dev/ipod0`。

### 1.3 mm-ipod 与 driver 的 ABI 调用关系

R44 的原厂 USB Transport ABI 复原，使用原厂 `mm-ipod`、`ipod-drvr-iap2.so` 和 `ipod-transport-usbdevice.so` 三个 ELF 交叉核验，22 项静态机器码/符号/重定位检查通过。

其中与 owner 关系直接相关的机器指令：
- mm-ipod `module+0x20`：读取模块的 Transport/interface 指针；
- mm-ipod `owner+0x0c`：把 owner 指针写入模块对象；
- mm-ipod `init+0x14`：从模块接口取初始化函数；
- 随后通过 BLX 调用初始化函数。
- iAP2 driver 从同一 Transport interface 的 `+0x28` 和 `+0x2c` 调用 data_send / data_recv。

这证明 mm-ipod 不只是目录扫描器：它读取模块对象、注入 owner、调用模块初始化。动态加载的 ELF 在该宿主地址空间内执行，因此 driver 的 Resource Manager 实现最可能由 mm-ipod 宿主进程承载。

### 1.4 物理 Transport 与 iAP2 driver 是两层

原厂 USB 插件导出 `ipod_module`，模块 ABI 版本为 `0x0200`，Transport interface 名为 `ipod_transport`，包含：
- `+0x14` init
- `+0x18` link params
- `+0x1c` type
- `+0x20` capabilities
- `+0x24` audio path
- `+0x28` send
- `+0x2c` receive

iAP2 driver 通过 Transport interface 的 send/receive 槽收发物理链路数据。mm-ipod 的启动脚本当前选择 `-t usbdevice`。这条 USB Transport 不得被替换或停用。

### 1.5 DIO 使用的是 client 侧设备路径

MU1005 `dio_manager` 配置的 iap2.device 为 `/dev/ipod0`。其 `iap2_connect` 通过 `libiap2client.so.1` 调用，path 由 wrapper 的调用方传入，而不是硬编码在 dio_manager 内部。

`libiap2client.so.1` 依赖 `libstrm.so.1` 和 QNX MsgSend 家族；导出 `iap2_connect`、`iap2_disconnect`、`iap2_raw_msg_send` 等 API。故 DIO 是 iAP2 client，不是该设备节点的创建者。

## 2. 当前判断与置信级别

| 判断 | 级别 | 原因 |
|---|---|---|
| mm-ipod 是 iAP2 driver 模块宿主 | 高（静态 ABI） | dlopen/dlsym、driver 命名、模块对象 owner 注入及 init 间接调用 |
| ipod-drvr-iap2.so 参与 QNX 设备服务 | 高（静态导入证据） | ipod_resmgr_mount + iofunc + MsgReply/MsgWrite |
| Resource Manager 实际由 mm-ipod 进程承载 | 高（架构推导，未运行验证） | driver 作为 mm-ipod 动态模块执行 |
| 该服务精确注册路径为 /dev/ipod0 | 中（尚未 callsite 闭环） | DIO config 使用此路径，但 driver mount 参数/运行态路径尚未静态定位 |
| 车辆当前运行时 /dev/ipod0 的 PID | 未知 | 无目标车辆 pidin/mount 快照 |
| 蓝牙 RFCOMM 已连接到 mm-ipod Transport | 未证明 | 没有真实蓝牙 producer 到 Transport send/recv 的闭环 |

## 3. 一个必须保留的概念区别

`ipod-drvr-iap2.so` 内部出现的 `BluetoothTransportComponent` / `WiFiTransportComponent`，属于 iAP2 驱动内部的传输组件标记；`ipod-transport-<name>.so` 则是 mm-ipod 外层物理 Transport 插件 ABI。两者不能仅凭名字视为同一层。

因此不能从 driver 字符串中看到 BluetoothTransportComponent，就推断 mm-ipod 已有一个可选 `ipod-transport-bluetooth.so`，也不能把 `iap2.cfg [bluetooth]` 段当作外层 `-t` 物理插件选择器。

## 4. 下一步唯一有价值的静态突破

1. 对私有原厂 `ipod-drvr-iap2.so` 反汇编，追踪 `ipod_resmgr_mount` 的全部调用点及参数构造，确认设备节点名称/实例编号来源。
2. 对 `ipod_module` 的构造、init、destroy 回调和 driver 的 open/read/write/devctl handlers 做交叉引用，确认资源管理器的生命周期是否完全由 mm-ipod owner 控制。
3. 对 `libiap2client.so.1` 的 `iap2_connect` / `iap2_raw_msg_send` 做 ABI 与消息请求结构逆向，追踪其打开路径、句柄、QNX MsgSend server channel。
4. 用一次只读车辆快照核对 `pidin` 进程、`/dev/ipod0` 节点、mount/resource manager 信息与 DIO 实际 fd。只读取必要元数据，不 dump 内存、不抓取认证数据、不修改配置。
5. 继续保留原厂 `-t usbdevice` 和有线 CarPlay；无线物理 producer 另行研究，不改原厂启动参数。

## 5. 安全边界

- 不修改 `iap2.cfg`。
- 不修改 `mm-ipod` 启动项，不替换 USB plugin。
- 不伪造 Apple MFi 或认证状态。
- 不创建或安装未经 QNX 6.5 目标验证的插件。
- 当前不生成 SD 安装包。

**本轮结论：已把 /dev/ipod0 的提供者收敛到 mm-ipod 宿主内的 ipod-drvr-iap2.so Resource Manager 实现；精确挂载路径和车辆 PID 仍待闭环。**
`installable=false` / `safe_to_flash=false` / `vehicle_verified=false`。
