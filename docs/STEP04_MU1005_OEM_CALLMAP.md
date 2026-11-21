# 第 04 步：MU1005 原厂 SI / 蓝牙 / iAP2 / DIO 静态调用关系核验

**日期：2026-11-22 · 基线：2017 奥迪 A6L 45 TFSI quattro；`MHI2_CN_AU57x_K1006_2 / MU1005` · 仅只读研发审计，不可装车**

## 验收结论

- **第 04 步“原厂组件及静态调用关系确认”已完成**：从保存的 70 版 `app.img` 六个原厂 ELF 校验 SHA-256、ELF32 ARM EABI5、实际导入/导出、PLT 重定位和 ARM BL 指令；运行 `verify_step04.py` 得到 **47/47** 静态断言通过。此结论仅限下面明确列出的真实静态关系。
- **没有确认完整无线运行链**：蓝牙 RFCOMM 到 iAP2 的真实端点及发布者、SI 中何时以何参数启动无线 DIO、MFi 会话授权与 AirPlay/Wi-Fi handoff 均未证明。这些是第 05–08 步要突破的边，**不是第 04 步已经打通的功能**。
- QNX 6.5 原生编译与 QEMU 加载在 R23 已通过，**但那只验证研究模块，未在原厂 SI / DIO 进程中运行**。允许安装：**否**。

## 源文件基线（与 R29 70 版 app.img 独立提取记录核对）

| 文件 | SHA-256 前 16 位 | 本次离线原始校验 |
|---|---|---|
| `smartphone_integrator` | `2c61e7a17d5c77df` | 完全匹配 |
| `btstack` | `51ecbd44c3581bd6` | 完全匹配 |
| `dio_manager` | `e40b5bf9df3f9622` | 完全匹配 |
| `libiap2client.so.1` | `0811ab8d8f8c72ca` | 完全匹配 |
| `ipod-drvr-iap2.so` | `dda3d733bb62539f` | 完全匹配 |
| `libairplay.so` | `193a4fd9101ec2aa` | 完全匹配 |

**重要：** `ipod-drvr-iap2.so` 使用当前 70 版镜像提取的 `dda3…`，**不是**历史 R14 变体 `42fd…`。不要用不同哈希固件的内部地址写补丁。实车闪存实际哈希尚未现场核对。

## 一、可直接证明的程序调用/接口链

### 1. `smartphone_integrator`：子进程和 CarPlay 准入控制的入口

- 精确二进制存在 `CHILDREN_CARPLAY_EXEC`、`CHILDREN_CARPLAY_ENVS`、`dio_manager`、`IPL_CONFIG_DIR_DIO_MANAGER`；有 `CControllerInternalJob`、`CMediaConnectionServiceJob_TestCarplay`、`CMediaConnectionServiceEvent_CarplayTested`、`DEVICE_FLAG_CARPLAY_VERIFIED` 等状态/事件对象。
- `servmngt::startApp` 是真实动态导入；直接 ARM `BL` 位于 **`0x16364c` → PLT `0x114754`**，字节与重定位表吻合。
- **确认范围：** SI 内确有 CarPlay child 配置与启动 API 调用；**仍不等于**这一个 `startApp` 调用在无线认证时一定启动 `dio_manager`，事件参数、child 的环境变量和 owner 生命周期都没有动态证据。

### 2. `dio_manager` → `libiap2client.so.1` → QNX IPC

- `dio_manager` 的 `DT_NEEDED` 明确依赖 `libiap2client.so.1`。
- `iap2_connect` 的直接调用：**`0x17a094` → PLT `0x1127f0`**；`iap2_disconnect`：**`0x179e9c` → PLT `0x112814`**。
- `libiap2client.so.1` 的 `iap2_connect` 在 `0x2cec` 导出，`iap2_disconnect` 在 `0x2c58`；它真实导入 `open`、`MsgSendv`、`MsgSendsv`、`MsgSendvs`。说明 DIO 使用 QNX iAP2 客户端的系统 IPC，而不是自动把蓝牙字节当 `/dev/ipod0` 数据。
- 原厂 `ipod-drvr-iap2.so` 导出 `link_handle_iap2pkt @ 0x12e00`（4668 字节），引用 `ipod_resmgr_mount`。**但现有六份 ELF 不包含负责挂载该驱动的完整 `mm-ipod` 运行上下文，因此还不能从客户端 IPC 精确连到无线蓝牙设备端点。**

### 3. `dio_manager` → `libairplay.so`（实际 BL 指令）

| DIO 原厂调用点 | PLT 目标 | 原厂 API |
|---|---|---|
| `0x15ddbc` | `0x11243c` | `AirPlayReceiverServerCreate` |
| `0x15df68` | `0x112514` | `AirPlayReceiverServerSetDelegate` |
| `0x15df88`, `0x15dfbc` | `0x1123a0` | `AirPlayReceiverServerControl` |
| *间接指针* | GOT `0x1a732c` | `AirPlayReceiverServerSetProperty` |

- 以上 PLT/API 均可在 `libairplay.so` 的定义符号表中对应到真实函数。`SetProperty` 是 **`R_ARM_GLOB_DAT` 间接入口**，不能伪称已找到直接 BL 调用点。
- DIO 保留 `interfaceName`、`startServer`、`stopServer`、`carplay0` 等常量及原厂回调（`s_sessionCreatedCallback` 等），表明**原厂 DIO 已有接收器管理实现**；不能因此认定该固件已经提供无线发现、mDNS 或正确网络绑定。
- **修正旧研究资料中的不一致：** 本次校验的精确 `dio_manager` 不含 `uap0` ASCII 字符串。以前资料声称“该原件存在 `uap0`”并不成立；不能据此硬编码 `uap0`。

### 4. `dio_manager` ↔ 蓝牙服务：ASI 客户端存在，服务端未定位

- 原厂 DIO 含 `asi.connectivity.bluetooth.BluetoothSmartphoneIntegration` 和同名 Reply/Proxy，保留 `CBluetoothControllerJob_GetBTInterface`、`HeadUnitBTInterfaceAvailable` 等接口/事件；这是**可确认的蓝牙 ASI 客户端**。
- 原厂 `btstack` 含 `__tracing__channel_CON_BTSTACK_IA_RFCOMM` 和通用 RFCOMM 状态/日志，证明有 Bluetooth RFCOMM 基础设施。
- **关键边界：** 这份精确 `btstack` 中未找到 `IapDeviceServices`、`IapServices`、`/dev/iapDevice`、`BluetoothiAP2Link` 原文标记。因此不能把另一个 **MU0678** 的 `IapDeviceServices → /dev/iapDevice` 线路直接称为本车已验证路径。也不能因为没有这些字符串就断言本车必然不支持蓝牙 iAP2：可能另有负责的二进制进程/模块。

### 5. 原厂 iAP2 Wi-Fi 能力：静态字串存在，不代表当前启用

- 70 版 `ipod-drvr-iap2.so` 存在 `BluetoothiAP2Link`、`BluetoothTransportComponent`、`WiFiTransportComponent`、`RequestAccessoryWiFiConfigurationInformation` 等名称。
- 只能证明该 iAP2 驱动带相关协议处理分支。**不能当作[WiFi] enable 已开启、认证已通过、AP 频段支持 5 GHz 或 iPhone 已能连入的证据。**

## 二、按原厂实证绘制的调用关系

```text
smartphone_integrator (SI)
  ├─ CarPlay child 配置：CHILDREN_CARPLAY_EXEC / dio_manager
  └─ 原厂 servmngt::startApp 的直接 BL (0x16364c)
                     │ [具体无线触发事件/入参：未证明]
                     ▼
               dio_manager (DIO)
          ┌──────────┼─────────────────┐
          ▼          ▼                 ▼
   libiap2client  libairplay.so   BluetoothSmartphoneIntegration ASI proxy
      │             │                 │
      ├─ MsgSendsv  ├─ Create         └─ 对端 ASI 服务提供者：尚未确认
      └─ QNX IPC    ├─ SetDelegate
          │         ├─ Control
          │         └─ SetProperty (GLOB_DAT)
          ▼
   QNX iAP2 资源管理器（mm-ipod/驱动挂载端点尚未连通）
          │
          ▼
   ipod-drvr-iap2.so: link_handle_iap2pkt (0x12e00)
          ╳  蓝牙 RFCOMM 到该资源端点：当前无真实跨进程调用证据
          │
   btstack: 发现通用 RFCOMM，但未定位此固件的 iAP2 服务端
```

图中有实线含义的边，均以真实 ELF 导入、重定位或 ARM BL 校验为基础。符号/字符串本身不等于运行时已触发。

## 三、第 05–08 步需要承接的准确缺口

1. **第 05 步**：找出本版固件实际的蓝牙 iAP2 RFCOMM 服务和端点创建者、发布者；不能沿用 MU0678 的路径假设。优先从已保存 70 版镜像的 **蓝牙高层服务、`mm-ipod`、iAP 组件** 继续提取。
2. **第 06 步**：证明 SI 真实事件如何生成合法 MFi 认证结果、设备身份与 CarPlay child 准入；当前 `CARPLAY_VERIFIED` 是状态位，不是 MFi 证书。
3. **第 07 步**：只读核对同版 `/etc/mm/iap2.cfg` 和真实 WLAN/AP 服务，定位蓝牙到 Wi-Fi 的会话切换。不得从驱动里有 Wi-Fi 字串推断已启用。
4. **第 08 步**：证明 SI/DIO 的唯一 owner、实际 AirPlay 监听接口、Bonjour/mDNS、配对身份及会话反馈形成同一代际。

**第 04 步判定：静态调用关系审计通过；无线完整链仍是未证实状态（此阻塞已归入后续步骤）。** 不提供固件补丁、覆盖原厂服务或 SD 安装器。实车继续留在固定第 13 步。

## 本地复现

将六个原厂 ELF 放入仅在本地保留的目录（**不要提交到公开 GitHub**），运行：

```bash
python3 verify_step04.py --oem-dir /path/to/private-oem-dir --output step04_audit.json
```

审计程序严格核对六份 SHA-256，统计 47 项 ELF/PLT/调用点/字串静态断言；校验失败会返回非 0。结果公开版本只保留 SHA、偏移、API 和真假状态，无固件原始字节。

参考资料（**只作跨固件架构借鉴，不当作 MU1005 实证**）：
- [luka-dev MHI2 MU1439 iAP2 路线](https://github.com/luka-dev/mib2q-carplay-rgi/blob/main/docs/reference/MHI2_MU1439_PORT.md)
- [joeyQuery MHI2 无线 CarPlay 静态研究](https://github.com/joeyQuery/MHI2-wirelessCarPlay/blob/main/docs/roadmap.md)
- [Roland8810 MH2p Bluetooth iAP2（不同平台）](https://github.com/Roland8810/MHI2-Wireless-CarPlay/blob/main/mh2p/bluetooth-iap2.md)