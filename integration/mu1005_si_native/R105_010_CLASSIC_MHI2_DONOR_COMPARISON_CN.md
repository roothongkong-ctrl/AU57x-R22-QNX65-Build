# R105.010｜经典 MHI2 无线 CarPlay 开源路线对照（MU1440 / MHI2Q / MH2p）

日期：2026-11-26
本车目标：MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
状态：PUBLIC_DONOR_REVIEW / STATIC_ONLY / NO INSTALL

## 1. 本轮检查的公开工程

| 项目 | 目标平台 | 可复用价值 | 不能直接移植的原因 |
|---|---|---|---|
| luka-dev/mib2q-carplay-rgi | MHI2Q MU1316/P5087，QNX6.5 ARM | iAP2 LinkEngine、RFCOMM graft、MFi/Wi-Fi bootstrap、AirPlay receiver lifecycle 的实现参考；R103 已锁定复用 LinkEngine | MHI2Q 不是本车 classic MHI2；其无线 graft 和 radio/ELF hook 不能直接装入 MU1005 |
| harman-f/mhi2_altscreen_carplay 的 frozen native-wireless research | classic MHI2 MU1440，QNX6.5 ARM32 | 最接近本车架构的服务图、iAP2/Type-3、MFi/ACP、AP credential、Bonjour/Controller 缺口研究 | MU1440 与本车 MU1005 不同固件；该研究明确是冻结实验原型，没有完整真车无线 CarPlay 验收 |
| joeyQuery/MHI2-wirelessCarPlay | MH2p AUG33 P2873，QNX6.6 | 原厂 btstack SDP/RFCOMM、/dev/iapDevice-*、ASI 路径、iAP2connectionmanager、Wi-Fi handoff 的完整架构参照 | MH2p/QNX6.6/不同蓝牙栈；不能照搬二进制、地址、结构体或路径 |

## 2. 最有价值的新发现：经典 MHI2 有一条“原厂 Type-3”候选路线

MH2p 公开静态研究描述的原厂链路：

```
iPhone
  -> btstack 的 iAP2 SDP/RFCOMM
  -> QNX Resource Manager 字节设备 /dev/iapDevice-<BT MAC>
  -> ASI BluetoothMediaBridge 向 SI 发布设备路径
  -> SI 子进程 iap2connectionmanager 打开该设备
  -> 原厂 iAP2 / MFi
  -> 5702/5703 Wi-Fi 凭据交换
  -> Bonjour / CarPlayControl
  -> AirPlay 会话
```

classic MHI2 MU1440 研究还记录了 mm-ipod 的通用 v2 `ipod_transport` 插件机制和 Type-3 transport 接口。它提示我们：**不要过早假定必须在蓝牙 RFCOMM 上再跑一套自建 LinkEngine；应先确认本车 ipod-drvr-iap2.so 是否能通过原厂 Type-3/transport component 接受一条受控的 RFCOMM 字节流。**

这与本车现有证据有联系，但没有形成证明：
- 本车 `ipod-drvr-iap2.so` 是 QNX ARM32 原厂 driver，包含 Resource Manager 相关导入及 Bluetooth/Wi-Fi transport component 标记。
- 本车 `mm-ipod` 使用外层 `ipod-transport-<name>.so` 模块 ABI，当前原厂启动选择 `usbdevice`。
- 本车 `dio_manager` 侧配置使用 `/dev/ipod0`，client 通过 `libiap2client.so.1` 与 Resource Manager 通信。
- 本车 `btstack` 的 iAP2 RFCOMM 服务注册调用参数尚未证明；R89 只证明 HFP/SIM 调用第三参数分别为 7/4。
- 本车蓝牙栈是否能安全发布每个配对设备的原始 RFCOMM 设备节点、ASI 是否有现成路径通知、Type-3 driver 是否能接纳该数据源，仍未闭环。

所以 MU1440/MH2p 只作为“架构 donor”，不是本车已成功实现的事实。

## 3. 三条路线重新排序

### 路线 A（优先研究）：原厂 Type-3 + RFCOMM 字节端点
目标：保留原厂 iAP2/MFi/ACP 处理，让一个受控的原厂兼容 Type-3 Transport 将蓝牙 RFCOMM 字节送进现有 iAP2 driver。

必须先证明：
1. 本车 mm-ipod 外层 plugin ABI 的 init/send/receive/destroy 完整调用约定。
2. driver 内部 BluetoothTransportComponent 的创建条件、数据回调和所有权。
3. 原厂 btstack 的 RFCOMM callback 与设备节点/ASI 发布链是否存在可复用入口。
4. 每个 peer 的地址、配对身份、generation 与 transport handle 能够一致绑定。
5. 有线 USB transport 在整个无线接入和回滚中保持不变。

只有以上证据对齐后，才编写本车 Type-3 适配器；不能把 MH2p 的 `/dev/iapDevice-*` 当成 MU1005 已有接口。

### 路线 B（并行保留）：R103 LinkEngine + R98 原厂 BlueSDK 接入
R103 已通过 host 测试，可继续作为独立协议层候选。R98 现在只允许 worker 预建 nonblocking fd，再由 owner 线程接管；legacy blocking bind 已 fail-closed。

只有在证明原厂 Type-3 路线不可用或无法复用 stock iAP2 状态机时，才继续把 R103 作为自建 iAP2 LinkEngine 的主路径，避免两套 iAP2 链路层同时接管同一连接。

### 路线 C（不采用）：盲改开关 / 替换原厂 btstack / 直接换用 donor 二进制
`enableIap`、`[bluetooth]` 配置、内部组件字符串、单一 SDP 记录都不能单独证明服务节点和真实字节通路。不得用跨固件 ELF 或猜测的 RFCOMM profile 参数替代调用链证据。

## 4. Wi-Fi 双频：修正“默认 5G”的证据要求

Luka 的 MHI2Q MU1316 公开实车笔记记录其无线电扫描只有 2.4 GHz；该结果只适用于该 donor 设备，不能推断本车 MU1005。该项目还指出 Apple CarPlay AP 识别依赖 Apple vendor information element 的 CarPlay capability 位，2.4 GHz 并非 iOS 自动拒绝的原因。

对本车的实施规则：
- 继续保留 2.4/5 GHz 双频状态机和失败后回退逻辑，但只有实际驱动/射频能力报告确认的频段才可进入候选。
- 当前不能宣称本车 5 GHz 可用，也不能因为 MHI2Q donor 只有 2.4 GHz 就断言本车只能 2.4 GHz。
- SSID 可见、Wi-Fi 热点能打开、iPhone 能连入，不等于 Apple vendor IE 已正确广播，也不等于 CarPlay association/Control session 成功。
- 先做 MU1005 只读 radio/AP 能力盘点：驱动型号、固件、channel list、hostapd/uaputl 命令能力、现有 AP channel/band、vendor IE 读回接口。不要先改国家码、信道或射频配置。

## 5. 下一轮实际工作清单

1. 从本车 ELF 审计数据恢复 mm-ipod 外层 plugin ABI 的所有槽位、调用约定、生命周期和当前 usbdevice 插件实现。
2. 对 `ipod-drvr-iap2.so` 中 `BluetoothTransportComponent`、`WiFiTransportComponent`、`ipod_resmgr_mount`、send/receive dispatch 建立交叉引用表，确认内部组件与外层 plugin 的关系。
3. 将 `mm-ipod`、driver、usbdevice plugin 三者按对象 owner/函数指针/回调调用关系对齐；不动原厂 USB。
4. 对 btstack 的 RFCOMM callback 和 ASI BluetoothMediaBridge 搜索真正的 raw stream producer；不以 HFP/SIM 线程替代 iAP2 owner。
5. 将 Type-3 donor 路线与 R103 LinkEngine 路线做接口成本和风险比较，选定唯一数据面 owner。
6. 单独恢复 libiap2client QNX MsgSend request/server dispatch ABI；这是 DIO client IPC，不能与外层 RFCOMM 传输混为一谈。
7. 最后才推进 MFi、5702/5703、Wi-Fi、Bonjour/Controller Connect、AirPlay iAP relay 和媒体会话。

## 6. 关键结论

本轮没有找到可直接刷入 MU1005 的完整无线 CarPlay 软件，也没有发现可以安全打开一个配置开关就完成无线 CarPlay 的证据。

**新的优先方向是：先查本车原厂 Type-3/driver 是否已经提供可接入的 iAP2 传输层，避免重复实现 stock 已经拥有的 iAP2/MFi 功能；R103 保留为备用协议路线。**

installable=false / safe_to_flash=false / vehicle_verified=false / mfi_apple_verified=false。
