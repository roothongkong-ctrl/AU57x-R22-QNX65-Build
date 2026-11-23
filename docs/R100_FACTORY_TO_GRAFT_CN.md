# R100｜同意启用软件注入后，原厂 iAP2 → btstack 软件接入双路线判定

**固件**：2017 A6L C7 PA，`MHI2_CN_AU57x_K1006_2 / MU1005`。**性质**：只读原厂 ELF 审计、源码级运行前门槛、QNX 编译。**不是车机安装包；不能刷车。**

## 本轮做成了什么

1. **原厂 5 份真实 ELF 精确核对**。`tools/r100_mu1005_route_audit.py` 按文件哈希、ELF 类型和 ARM32 架构逐个核验 `btstack`、`bluetooth`、`ipod-drvr-iap2.so`、`smartphone_integrator`、`connectivity_launcher`，并记录内建字符串位置。原厂文件保留在私有环境，没有上传到 GitHub 或安装包。
2. **原厂功能与另一固件存在差异**。当前 MU1005 `btstack` 有 `RFCOMM`、`HfRfCallback`、`SimClientRfCallback`、`pthread_create`，但没找到 MU0678 等项目里命名的 `/dev/iapDevice`、`IapServices` 或 `BluetoothiAP2Link` 字符串。原厂 `bluetooth` 有 `bluetooth.enableIap`；原厂 `ipod-drvr-iap2.so` 有 `BluetoothTransportComponent`、`BluetoothiAP2Link`、`/etc/mm/iap2.cfg`。**没有命名字符串不是代码不存在的证据，只表明不能直接移植另一固件的端点与地址**。
3. **把用户授权的软件注入路线接入可执行代码的安全门槛**：新增 `bt_route_gate.[ch]`。当且仅当原厂服务有真实 endpoint 和 RFCOMM 证据时选择原厂；否则，在明确选择 graft、确认当前原厂进程、BlueSDK 线程、回调所有者、iAP2 注册参数、原厂服务已退出、备份、启动恢复和有线 CarPlay 保留后，才返回 `GRAFT_RUNTIME_ELIGIBLE`；其余全拒绝。
4. **接在已有 R98 真正的 QNX 原生 `au98_bind_stock_native()` 调用前**，不是单独的文档检查。即使 `LD_PRELOAD` 已加载，如果没有完整的原厂所有权/注册证据，就不会调用 R45 BlueSDK 注册适配器。R91 的实验 `RF_RegisterServerChannel arg=0` 仍是实验参数，不能冒充真实服务登记。此守卫由外部运行时证据提供者填充，当前**并无已验证提供者**。
5. **可复核的实际测试**：原厂 5 个 ELF 真实检查、7 项审计负例、17 项路由门槛断言、44 个 C 源文件严格编译、R98 真实环回 socket 回归、旧版测试回归。见 `r100_result`。

## 为什么不能直接交付装车包

虽已允许软件注入，仍没有明确的原厂 `connectivity.json` 原始内容，也没有原厂真实 BlueSDK iAP2 所有者事件线程、已验证的第三注册参数及原厂撤销/回滚所有者。仅凭编译、模拟回调、HFP 线程或 OEM ELF 的字符串不够；随意从普通线程调用原厂 RF 注册可能影响蓝牙通话或使系统重启。真正的 MFi 认证、无线 AirPlay 和 WI-FI 网络切换还不能离线证明。

## 路线决策（自动门槛，不凭空判成功）

- 原厂服务启动且原厂 Bluetooth→iAP2 RFCOMM/endpoint 都验证时：优先走原厂。
- 原厂证据不足但 **graft 的所有原生所有者、注册参数、恢复和保留有线条件全部满足** 时：可尝试软件 graft。
- 其余：`NO_GO`，不得安装/执行原厂蓝牙注册调用。回退原厂有线 CarPlay 优先。

## 下一处真正的工作

需要在现有原厂 `btstack` 程序上恢复 **BlueSDK 事件线程与 iAP2 服务注册的可调用时序**，不能再借 HFP 回调充当 iAP2 线程。随后把自动注册和撤销接入原厂真正启动器，再做 QNX 实车候选与恢复。实际 iPhone、MFi 和 AirPlay 连接仍需最后一次上车证实。缺实际配置时可做带只读自检的 SD 包，但不能误称其已可自动安装。

**进度结论：R100 完成路线切换的原厂证据审计和原生接入前安全门槛；无线 CarPlay 仍未实现。**
