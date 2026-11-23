# R101｜原厂 RFCOMM 注册参数与服务通道范围修复（离线源码，禁止刷车）

基线：MHI2_CN_AU57x_K1006_2 / MU1005，原厂 btstack SHA256：`51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566`。

## 本轮实际解决的两个代码问题

**1. 旧服务通道上限错误。** `inbound_publish()` 曾在注册返回后只接受 1～6 的 `slot`；但是 RFCOMM Server Channel Number（SCN）标准有效范围是 1～30。现在接受 1～30，允许 7、30，拒绝 0、31、255。成功注册之后把**真实返回 slot**写入 SDP `ProtocolDescriptorList` 的 RFCOMM channel（`protocol[13]`）。失败则标记 DIRTY，不再写/发布 SDP。模拟 BlueSDK 的真 C 发布/撤销测试覆盖边界值。

**2. 原厂第三参数与测试中的 0 值误用。** 以原厂 ELF 为证，HFP 原始调用 `0x2464b0` 前传 `r2=7`，SIM 原始调用 `0x24dcac` 前传 `r2=4`，两个 BL 都到 `0x277a18`。该函数先把 r2 保存至 r6，又把 r6 写进蓝牙通道对象偏移 `0x8b`（字节）与 `0x86`（半字）。因此此前将 7 / 4 **直接断言为服务类型/profile 枚举**没有足够依据；更可能是 RFCOMM 通道号或 channel-like 注册 hint。**尚未完全逆出此私有 API，也未找到 iAP2 专用值。**

`mu_rf_arg_choose()` 生产路径现在只接受经外部所有者证明的 **1～30**；无法证明的 0 不再被“verified”误用。R91 `AU57X_FAST_CONNECT_EXPERIMENT=1` 的 0 尝试仍保留**仅用于明确编译启用的实验**，不属于生产可安装包，也没有车机成功证据。

## R98→原厂函数的参数冻结

之前 R98 第一次检验参数后，`native_binding` 在注册时会**再次调用外部参数提供者**。如果其值发生变化，会产生检验—使用（TOCTOU）不一致。现在 `au98_publish` 在开启 TCP 连接前冻结这一次验证的值，把同一数值交给原生 `native_binding` 使用；原生调用前必须确认冻结标志和数值范围。测试新增参数提供者在绑定后发生变化、原生快照不变的例子。

这仅保证参数一致性；**不说明外部原厂所有者、认证或注册参数一定正确**。

## 真实原厂二进制静态审计

`tools/r101_rfcomm_scn_audit.py --btstack <私有原厂btstack路径> --output <json>`：只读检查精确 SHA、ELF32 ARM、13 条指令和两条 BL 目的地址。用户原厂文件不包含在 ZIP / GitHub；云端不假冒有原厂文件，执行合成 ELF 负例。

## 验证状态

- 本地真实原厂 ELF 指令审计 13/13 通过（只是静态）。
- 新增 SCN 0/1/6/7/10/30/31/255 发布-撤销边界 C 测试通过。
- `rf_arg_experiment` 严格和明确开启实验两种构建通过。
- 原 R98 loopback/绑定、R100 的 12 组原生源码/事务/有线优先回归通过，44/44 C 模块严格编译通过。
- 新 QNX ARMv7 CI 以 GitHub 最新工作流实际结果为准，无法把启动容器、ELF/QEMU 或测试等同于实车。

## 剩下的关键阻塞

1. 原厂 MU1005 iAP2 所有者线程 / BlueSDK 回调尚未找到可信的可运行入口。
2. 私有 `RF_RegisterServerChannel` 第三参数的完整 ABI、iAP2 SCN 与 SDK 内部锁未获独立证明；**不能按 7、4 或 0 猜出 iAP2 值**。
3. 蓝牙 iAP2 真实链路、苹果 MFi、Wi-Fi AirPlay、原厂开机恢复与 SD 装车安装都未验收。

**状态：完整工程源码继续研发，`installable=false`、`safe_to_flash=false`、`vehicle_verified=false`，不安排中途上车。**

**FIX1 修订**：将 R89 旧测试由“注册时再次读取外部 profile 回调”改为验证 R98 前置证明、R101 冻结快照与原厂调用使用同一数值。不会弱化原厂运行条件。
