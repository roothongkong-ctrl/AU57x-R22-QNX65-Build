# R105.012｜第 02 步：MU1005 原厂 ipod-drvr-iap2.so 内部结构静态分析

日期：2026-11-26
固定路线：22 步中的第 02 步
目标：2017 Audi A6L C7 / MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
状态：OEM_ELF_HASH_MATCH / ARM_DISASSEMBLY_REVIEWED / STATIC_FUNCTION_MAP_PASS / NO_INSTALL

## 1. 本轮真正拿到并分析了原厂 ELF

从用户此前保存的完整研发交接资料包中复用原厂二进制，不要求用户重新上传：
- 归档：`AU57x_C7_无线CarPlay_交给其他AI_一包全资料_20261112.zip`
- 成员：`02_原厂二进制_仅私有交接/ipod-drvr-iap2.so`
- 文件大小：739,925 bytes
- SHA-256：`42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf`
- Build ID：`f66b11f264233f156e4414ab8b438917`
- 格式：ELF32 / ARM / EABI5
- DT_NEEDED：`libstrm.so.1`、`libpps.so.1`、`libc.so.3`

原始 ELF 保留在私有交接资料中，未上传到公开 GitHub。公开仓库只保存 SHA、符号地址、反汇编交叉引用摘要和自动化证据门禁。

## 2. 已确认的原厂外层 Transport callback 分发关系

驱动实际通过 transport context 的函数表调用物理传输插件。反汇编显示：

```
driver context + 0x14
  -> transport context + 0x40
       + 0x18 : get_link_params()
       + 0x1c : type_get()
       + 0x20 : caps_get()       （可选能力槽）
       + 0x24 : audiopath_get()
       + 0x28 : data_send()
       + 0x2c : data_recv()
```

关键函数：

| 函数 | VA | 长度 | 静态确认 |
|---|---:|---:|---|
| `transport_receive` | 0x2111c | 40 B | 通过 transport 表 +0x2c 调用接收回调 |
| `transport_get_link_params` | 0x21144 | 24 B | 通过 +0x18 取得链路参数 |
| `transport_send_pkt` | 0x2115c | 192 B | 通过 +0x28 调用发送回调，并检查发送长度 |
| `transport_recv_pkt` | 0x2121c | 672 B | 取接收队列、提取 transport data、解析 packet、最终调用 iAP2 handler |

这与第 01 步从原厂 USB donor 映射的七槽 callback prefix 相互吻合。它确认的是 **iAP2 driver 到外层 transport plugin 的数据面 ABI 关系**，并不证明已经存在可用的蓝牙插件或 RFCOMM 入口。

## 3. Bluetooth 相关函数的真实职责

| 函数 | VA | 长度 | 交叉引用结论 |
|---|---:|---:|---|
| `bt_start_updates` | 0x553c | 372 B | 构造并发送 iAP2 消息 0x4E03 |
| `bt_send_info` | 0x56b0 | 328 B | 构造并发送 iAP2 消息 0x4E01 |
| `bt_update_recv` | 0x505c | 1,100 B | 解析蓝牙连接/配置文件状态更新，并发布 PPS 状态 |
| `bt_stop_updates` | 0x54a8 | 148 B | 停止蓝牙状态更新 |

关键调用：
- `iap2_start_features`（0x5fdc）调用 `bt_send_info`，成功后调用 `bt_start_updates`。
- `link_handle_iap2pkt`（0x12e00）在 callsite 0x13818 调用 `bt_update_recv`。

**重要修正：** 这些 `bt_*` 函数是 iAP2 协议中的蓝牙连接/配置文件状态与功能消息处理，不是 RFCOMM socket 读写函数，也不能拿它们冒充 BlueSDK RFCOMM callback。真正的蓝牙字节入口仍要从外层 transport plugin / BluetoothTransportComponent 的创建和接线关系中找。

## 4. Wi-Fi 相关函数的真实职责

`wifi_acc_config_info`：
- VA：`0x21b80`
- 长度：996 bytes
- 由 `link_handle_iap2pkt` 的 callsite `0x1382c` 调用。
- 构造 iAP2 消息 `0x5703`。
- 可观察到 SSID、Passphrase、SecurityType、Channel 等字段的解析/组包路径。

这证明驱动有 **Accessory Wi-Fi Configuration Information 的 iAP2 消息处理**。它不证明：
- 车机无线网卡支持 5GHz；
- 车机 AP 已创建；
- Apple vendor IE 已正确广播；
- iPhone 已成功关联；
- Wi-Fi transport 已作为有效 CarPlay 数据面启动。

## 5. Transport Identification 相关内部函数

已定位以下本地函数（非动态导出）：

| 函数 | VA | 长度 |
|---|---:|---:|
| `ident_info_tspbt` | 0x0d490 | 728 B |
| `ident_info_tspwifi` | 0x0abf8 | 728 B |
| `ident_info_tspusbdev` | 0x0b9b0 | 920 B |
| `ident_info_tspserial` | 0x0a914 | 60 B |
| `ident_info_recvmsgs` | 0x0bd48 | 1,804 B |
| `ident_info_sendmsgs` | 0x0c454 | 1,820 B |

Bluetooth/Wi-Fi Identification helper 都通过 transport callback 的 `type_get` / 链路参数接口读取当前 transport 信息，并组装 iAP2 Identification 数据。它们说明驱动存在按 transport 类型区分的识别逻辑，但不能单独证明底层 Bluetooth/Wi-Fi 组件的构造函数、vtable 或 socket producer。

## 6. 组件与配置元数据

在 ELF 中确认存在以下字符串/元数据标识：
- `SerialTransportComponent`
- `USBDeviceTransportComponent`
- `USBHostTransportComponent`
- `BluetoothTransportComponent`
- `WiFiTransportComponent`
- `BluetoothTransportComponentIdentifier`
- `NativeTransportComponentIdentifier`
- `TransportComponentIdentifier`
- `TransportComponentName`
- `TransportSupportsiAP2Connection`
- `TransportSupportsCarPlay`
- `BluetoothTransportMediaAccessControlAddress`
- `ConnectedBluetoothProfiles`

这证明 ELF 包含这些组件名称及属性元数据。当前静态证据尚未还原完整 constructor/vtable 和配置映射执行路径。因此，早期研究笔记中“仅凭 iap2.cfg 的 [transport] 段就能选择/启用 BluetoothTransportComponent 与 WiFiTransportComponent”的说法，必须降级为**待验证假设**，不能作为实施依据。

另外，`Doesn't support WIFI` 字符串存在，但其触发分支/编译条件尚未定位；不能据此断言 Wi-Fi 功能整体关闭。

## 7. 本步结论与下一步边界

第 02 步的**原厂 driver ELF 身份确认、关键函数静态反汇编和内部职责图**已完成。

目前得到的最重要结论：
1. iAP2 driver 的通用物理数据面明确通过外层 Transport callback table 的 send/receive 槽工作。
2. `bt_*` 处理的是 iAP2 Bluetooth 状态/功能消息，并非 RFCOMM 数据 socket。
3. `wifi_acc_config_info` 处理的是 iAP2 Wi-Fi 配置信息消息，不是无线电/AP 管理器。
4. Bluetooth/Wi-Fi component 名称和 Identification helper 存在，但其 constructor/vtable、真实 transport 选择规则和 BlueSDK RFCOMM producer 仍未闭环。

### 第 03 步应继续追查

- BluetoothTransportComponent 的构造/注册函数、vtable 与 callback 实现；
- WiFiTransportComponent 的构造/注册函数、vtable 与实际 radio/AP 调用；
- 组件元数据表（`0x283xx`、`0x288xx`、`0xa8dxx` 一带）与函数指针的交叉引用；
- `ipod_resmgr_mount` 及 resource manager read/write/notify 路径如何接入 transport；
- 本车 BlueSDK RFCOMM 原始字节流是否能进入这套 driver，而不是只上报 profile 状态。

验收：
- exact OEM ELF SHA：PASS
- ARM32 symbol/disassembly review：PASS
- transport send/receive dispatch map：PASS
- Bluetooth feature/status map：PASS
- Wi-Fi config message map：PASS
- BT RFCOMM live data path：NOT PROVEN
- component constructor/vtable：NOT PROVEN
- MFi/iPhone/runtime：NOT TESTED
- vehicle verified：false
- installable：false
