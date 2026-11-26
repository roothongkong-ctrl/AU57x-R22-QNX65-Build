# R105.014｜第 04 步：MU1005 原厂 Bluetooth / btstack iAP2 owner 边界审计

日期：2026-11-26
22 步路线：第 04 步
目标：2017 Audi A6L C7 / MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
状态：STATIC_OWNER_BOUNDARY_MAPPED / OEM_IAP2_OWNER_NOT_PROVEN / NO_INSTALL

## 本轮先复用已有成果，未重复做 40/40 检查

本轮复用此前原厂 ELF 精确 SHA、R88 40 条 ARM 指令核验、R101 HFP/SIM RFCOMM 参数审计、R45 server-first ingress、R92/R93 pthread interposition 结构研究和 R98 owner handoff。没有重新跑同一批 40 条 ARM 指令，也没有把 HFP=7 或 SIM=4 当成 iAP2 SCN。

原厂 ELF 仍只保留在用户私有资料中；公开仓库只保存 SHA、符号地址、接口名称和审计结论。

## 1. Bluetooth 应用层：确有 iAP 控制配置，但它不是 RFCOMM 数据 owner

原厂 `bluetooth`：
- SHA-256：`7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272`
- ELF32 ARM EABI5 / QNX，1,410,944 bytes。
- 原始字节中存在 `bluetooth.enableIap`（offset `0x1359ac`）、`iapEnabled`（`0x1339b0`）、`SERVICETYPE_IAP2`（`0x13cc2c`）。
- 有 `CBluetoothSmartphoneIntegration` 的 36-byte vtable（VA `0x256698`）、对应的 ASI `BluetoothSmartphoneIntegrationServiceRegistration` vtable（VA `0x252008`，64 bytes）和 interface ID 数据。

进一步交叉引用发现它导入 `comm::ServiceRegistration::registerService()` / `unregisterService()`。因此这里确认的是 **Bluetooth app 对外发布 SmartphoneIntegration DSI/ASI 控制服务的框架**，并且程序包含 iAP2 开关/服务类型标记。它没有直接证明该类创建了 btstack 内的 iAP2 RFCOMM channel，也没有给出 RFCOMM callback。

## 2. btstack：有通用 SDP / RFCOMM 基础设施，未定位 iAP2 专属 owner

原厂 `btstack`：
- SHA-256：`51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566`
- ELF32 ARM EABI5 / QNX，1,846,436 bytes。
- 动态符号中可见 `BluetoothServiceBase` vtable（VA `0x2be7a0`，72 bytes）、`BtJobStartService` vtable（`0x2bec90`，56 bytes）、`BtJobQuerySdp` vtable（`0x2be908`，56 bytes）。
- `BtJobQuerySdp::queryDeviceCallback` 位于 `0x137764`，308 bytes；`BtJobStartService::cmgrEvent` 位于 `0x13e350`，weak symbol，4 bytes。
- RFCOMM tracing channel `__tracing__channel_CON_BTSTACK_IA_RFCOMM` 位于 `0x2d5724`；字符串表有 RFCOMM 信用额度、CFC、断线错误等协议诊断。

这些符号说明原厂 btstack 具备通用 Bluetooth service、SDP 查询/启动作业和 RFCOMM 协议基础设施。它们不是 iAP2 专属 callback 的证据。当前动态符号表没有给出可直接使用的 iAP2 RFCOMM callback 原型；`IapDevice`、`IapServices`、`/dev/iapDevice`、`CIapBTChannel` 等 MU0678 donor 字面标记也未在这份 MU1005 btstack 样本中找到。缺少字面标记不等于动态构造路径不存在。

## 3. iAP proxy：是跨进程 RPC，不是原始蓝牙字节通道

`libasimmxconnectivity_bluetooth_iapproxy.so`：
- SHA-256：`44883ed28e44a8f06b5308266510981e17b3ee59d737f73208cd76e11ffc1c80`
- 存在 `asi.connectivity.bluetooth.iap.IapDeviceServices`、`IapDeviceServicesRPCStub`。
- 该库是 ASI RPC proxy/stub 侧证据。不能把它当成 RFCOMM socket、iAP2 Transport plugin 或 mm-ipod 的 send/receive callback。

## 4. 与已有桥接代码的关系

已有 R45 server-first ingress 能接收一个**已经真实发生**的 RFCOMM event，并转换为带 generation 的本地帧；R98 handoff 只允许由真正的 iAP2 owner 提供 owner/service/paired-peer 证明后接管。它们都没有自行创造 OEM owner 证明。

本轮重新核对后，关键缺口仍是：
1. `bluetooth.enableIap` / `iapEnabled` 怎样触发 btstack 中的实际 iAP2 服务创建；
2. iAP2 专属 `RF_REGISTER` / `SDP_ADD` 调用者、SCN 和持久 channel object；
3. RFCOMM callback 的真实函数原型、owner 线程、锁/重入约束、packet buffer 所有权和 teardown barrier；
4. callback 怎样进入 R45 ingress，及如何保持同一配对手机身份和连接代数；
5. raw RFCOMM bytes 怎样进入 mm-ipod 的 transport +0x28/+0x2c，再到 iAP2 driver。

R101 的 HFP=7 / SIM=4 只说明不同服务使用不同注册参数；第三参数在已分析调用中体现为 receive-credit seed，不能推导 iAP2 值。不得猜 iAP2 SCN、信用额度或函数签名。

## 5. 本轮新增交付与测试

- `docs/R105_STEP04_BTSTACK_IAP2_OWNER_STATIC_AUDIT.json`：只记录原厂 ELF SHA、符号/地址、字面标记与边界结论。
- `tests/test_r105_step04_btstack_iap2_owner_audit.py`：5 项 host 静态证据门禁，检查 ELF 指纹、Bluetooth 控制层与 RFCOMM 数据层区分、proxy 边界、owner 未证明状态。
- 已接入 R105 GitHub Actions。测试通过仅表示本轮审计记录没有把未证明事项写成事实，不表示 OEM ELF 在 QNX 上执行或 iPhone 已连接。

## 6. 第 04 步结论与下一步

**第 04 步的离线 owner 边界审计完成；实际 iAP2 RFCOMM owner 仍 BLOCKED。**

这是一个有效的排除和收敛结果：不能再把 Bluetooth app 的 `enableIap` 标记、btstack 的通用 RFCOMM tracing、SDP job 或 ASI iAP proxy 当成真正的 iAP2 数据 owner。

第 05 步继续沿 SI/ASI 与原厂 iAP endpoint 查证：
- `smartphone_integrator` 对 Bluetooth SmartphoneIntegration 服务的真实调用/订阅关系；
- `libirc_mmx_adapter.so` 的缺失符号和实际 ImageFS 来源/装载边界；
- `libasimmxconnectivity_bluetooth_iapproxy.so` 的 RPC 方法表与 endpoint 身份；
- SI、DIO、`libiap2client.so.1` 的路径参数如何传入，以及是否有任何真实蓝牙 endpoint producer。
目标是确定 SI 能否合法地从原厂 iAP 服务取得同一手机会话，而不是创建第二个未经认证的 iAP2 owner。

验收：
- original Bluetooth/BTStack/iAP proxy hashes：PASS
- iAP enable/service markers：PASS_STATIC
- generic SDP/RFCOMM infrastructure：PASS_STATIC
- iAP2-specific RFCOMM owner / SCN：NOT_PROVEN
- callback ABI/thread/buffer lifecycle：NOT_PROVEN
- R45/R98 production owner binding：NOT_PROVEN
- QNX runtime / iPhone MFi / vehicle：NOT_TESTED
- installable / safe_to_flash：false

## GitHub 自动门禁实测（2026-11-26）

R105 host-only workflow run `45764929332` 已完成 SUCCESS。第 04 步新增的 5 项审计门禁全部 PASS：
- 三份原厂 ELF 指纹固定；
- Bluetooth app 的 iAP 标记与 DSI 服务类被正确归类为控制面；
- btstack 的 SDP/RFCOMM job 与 tracing 被正确归类为通用协议设施；
- ASI iAP proxy 未被误称为 RFCOMM 原始字节传输；
- 所有未证明的 iAP2 owner / SCN / callback / installable 状态继续 fail-closed。

本次 CI 是对静态审计记录的主机检查，不执行原厂 QNX ELF，不证明真实 owner 已绑定。
