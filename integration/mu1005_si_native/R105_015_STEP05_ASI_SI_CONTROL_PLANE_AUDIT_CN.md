# R105.015｜第 05 步：ASI / SmartphoneIntegration 控制面与 iAP2 数据面分离

日期：2026-11-26
22 步路线：第 05 步
目标：2017 Audi A6L C7 / MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
状态：ASI_CONTRACT_STATIC_VERIFIED / RAW_BYTE_BRIDGE_CLOSED / DATA_PLANE_REDIRECTED / NO_INSTALL

## 本轮的关键原则：先复用第 25–27 轮，不再重复拆同一个代理库

本轮检索并复核用户历史资料中的第 27 轮完整 ASI 接口审计，以及第 25 轮依赖审计和 app.img 取件记录。第 27 轮已经拿到原厂固件自带的完整 EMS 方法契约、扫描了 app.img 的 778 个 ARM ELF，并得出 ASI IapDeviceServices 只有 proxy/stub、没有服务端实现的结论。因此本轮不再重复反汇编 48 KB proxy，也不再尝试用 `stringRequest` 猜测裸字节传输。

## 1. IapDeviceServices 方法契约：完整 9 项，全部是设备/状态管理

来源：`asi.connectivity.bluetooth.iap.ems_table.json`，接口版本 1.0.1；位于 app.img offset `0x0F1D2989`，JSON 从 `0x0F1D29B6` 开始，2,207 bytes。

| ID | 方法 | 参数 | 回复 |
|---:|---|---|---|
| 0 | requestReconnectDevice | dev: string | 有 |
| 1 | responseReconnectDevice | dev: string, result: Result | 有 |
| 2 | updateActiveDevice | dev: string | 有 |
| 3 | stringRequest | str: string | 无 |
| 4 | stringResponse | str: string | 无 |
| 5 | updateActiveDevice | dev: string, address: int64 | 有 |
| 6 | updateLocalBtAddress | address: int64 | 无 |
| 7 | updateLocalFriendlyName | name: string | 无 |
| 8 | updateActiveDevices | IapDevice[]（devPath/address/inUse） | 无 |

ID 3/4 虽然是字符串参数，但没有请求-应答配对、分片、序号或字节数组类型；不能据此把它们解释为 iAP2 packet tunnel。ID 8 明确使用结构化数组，进一步说明 EMS 契约具有结构化参数表达能力，而这份接口没有定义原始帧或流接口。

## 2. Proxy 库与服务端定位

原厂 `libasimmxconnectivity_bluetooth_iapproxy.so`：
- 大小 48,248 bytes；SHA-256 `44883ed28e44a8f06b5308266510981e17b3ee59d737f73208cd76e11ffc1c80`。
- 只有 `getProxyStubFactory` 导出，含 PROXY/STUB/unknown method ID 等生成式 RPC 字符串。
- 第 27 轮对 70 版 app.img 的 778 个 ARM ELF 扫描：接口实现修饰符号只命中 proxy 本身；没有发现另一个 ELF 依赖该 proxy，也没有发现对应的 provider 实现。
- btstack 不含该 iAP 接口的服务端实现；DIO 与 SI 的方法表也不是原始 iAP2 数据接口。

边界：这是对所检查的 70 版 app.img 的完整 ELF 清单结论，不外推为所有固件分区或运行时文件系统绝对不存在其他提供者。

## 3. BluetoothSmartphoneIntegration 与 smartphone_integrator 是控制面，不是 packet transport

原厂 `BluetoothSmartphoneIntegration` EMS 表共 9 个方法：
- reportConnectionEstablished
- reportParingSuccess
- reportSharedSecret
- requestLocalBluetoothAddress
- requestPrepareConnect
- responseLocalBluetoothAddress
- responsePrepareConnect
- updateSmartphoneMode（含 CARPLAY 等模式）
- updateSmartphoneMode 的 CARLIFE 变体

这些方法负责蓝牙连接/配对/口令/准备连接/手机模式状态。

原厂 `dsi.smartphoneintegration.ems_table.json` 共 46 个方法，涵盖 connect/disconnect、设备发现、startService、设备信息、Now Playing 元数据、封面 URL、触控、字符、旋钮和电话状态。唯一带 `Data` 字样的 `updateNowPlayingData` 是歌曲元数据，不是媒体字节流。该方法表没有 iAP2 raw frame / byte stream 接口。

## 4. libirc_mmx_adapter.so：修正第 25 轮旧结论，但不改变数据面判断

历史第 25 轮根据 app.img ELF 动态符号扫描，曾误判 `libirc_mmx_adapter.so` 不在 app.img。第 27 轮随后使用已打通的 QNX6fs 目录项解析，按文件名从 app.img 直接取出该库，并通过 SHA256SUMS 固定身份：
- 大小 296,740 bytes；
- SHA-256 `3277e0c410ef3be3ea8d752ccd142c706e156e7a621f69ea95e6dd174b2ba9b2`；
- 定义 SI 缺失的三组 `SmartphoneIntegration_AppState / MetaInfo / LauncherLog` 符号及相关 dumper vtable。

因此，旧的“只能在 mifs-stage2”判断已被第 27 轮新证据推翻。现在应以 app.img QNX6fs 按名取件结果为准。**但该库只是 smartphone/IRC adapter 依赖，不会凭空提供 iAP2 raw-byte server，也不改变 ASI 接口没有 provider 的结论。**

## 5. 本轮决策：终止 ASI 裸字节桥，避免继续浪费研发轮次

正式关闭以下错误路线：
- 不把 `IapDeviceServices.stringRequest/stringResponse` 当作 RFCOMM 数据管道；
- 不把 BluetoothSmartphoneIntegration 的连接状态/配对成功/模式切换当作 iAP2 packet；
- 不把 smartphone_integrator 的 UI/触控/Now Playing DSI 当作 iAP2 transport；
- 不再为 ASI proxy 设计 raw frame 注入层；
- 不因补齐 `libirc_mmx_adapter.so` 就声称无线 iAP2 已具备。

这不是放弃无线 CarPlay，而是把开发资源从不存在的 ASI 裸数据桥移到正确的数据面。

## 6. 下一步路线：原厂 DIO / AirPlay / iAP2-over-network

下一条主线是：
`iPhone Bluetooth discovery/pairing` → `Wi-Fi provisioning / network reachability` → `dio_manager` → `libairplay.so` + `libiap2client.so.1` → `/dev/ipod0` / 原厂 iAP2 driver → 原车显示/音频/控制。

必须继续逐项验证：
1. `dio_manager` 如何读取并选择 `/dev/ipod0`，以及调用 `libiap2client.so.1` 的实际函数和参数；
2. AirPlay receiver 的服务端启动、Bonjour 发布、监听 socket/端口及 session callback；
3. iAP2-over-network 与蓝牙配对设备如何绑定到同一个 iPhone；
4. Wi-Fi provisioning 使用哪个原厂接口；2.4GHz 先作为基础路径，5GHz 保留探测和回退，但不在缺乏硬件证据时强制启用；
5. USB CarPlay 优先级、断线清理和原厂恢复必须保持 fail-closed。

## 7. 第 05 步验收

- IapDeviceServices EMS 9 项契约：PASS_STATIC
- ASI proxy identity / generated proxy-stub boundary：PASS_STATIC
- 70 版 app.img 778 ELF provider scan：NO_PROVIDER_FOUND（历史完整扫描证据）
- BluetoothSmartphoneIntegration 9 项状态方法：PASS_STATIC
- smartphone_integrator 46 项 UI/session/input/metadata 方法：PASS_STATIC
- libirc_mmx_adapter.so 文件身份及三组 SI 符号：PASS_STATIC（第 27 轮证据）
- ASI raw-byte bridge：CLOSED
- DIO/AirPlay/Wi-Fi data plane：NEXT / NOT YET PROVEN
- QNX runtime / iPhone MFi / vehicle：NOT_TESTED
- installable / safe_to_flash：false

**第 05 步完成的是接口契约和错误路线排除，不是无线 CarPlay 已连通。**
