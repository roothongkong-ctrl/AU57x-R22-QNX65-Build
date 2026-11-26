# R105.005｜跨分支成果核对：MU1005 无线 CarPlay 原厂链路收敛

日期：2026-11-26
基线：MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
工作分支：`mu1005-si-cprx-real-review`
状态：SOURCE_REVIEW_ONLY / NO_INSTALL / vehicle_verified=false

## 1. 本轮核对了什么

不是重新做 R105.004 的文件盘点。本轮对比了同仓库的 R103 原厂 iAP2 路线、R143 原厂线程联合观察器，以及较新的 SI/CPRX、AirPlay 第二段传输和 AP live 分支，目的是统一真实数据路径，避免多个实验分支重复实现。

核对的主要成果：
- R88/R97/R98/R100：原厂 btstack/RFCOMM 候选、事件入口和 MFi 保留门禁；
- R99：蓝牙 JSON 配置候选生成器及后续策略修正；
- R103：严格复用 LuKa 上游 iAP2 LinkEngine；
- R142/R143：只读观察 BlueSDK 线程命名，并将线程创建来源与线程名相关联；
- SI native registration/activation/owner：原厂 SI 记录注册和激活适配；
- R81–R85：SI 与 DIO 分属两个原厂进程时的 CPRX 跨进程 START/STOP、会话状态和有线恢复屏障；
- inner_iap2：原厂 AirPlay event/control 第二段 iAP2 字节桥；
- ap_live：只读查询原厂 uaputl 的 AP 配置并生成当前无线会话参数。

## 2. 重要纠偏：R99 两配置开关旧结论不能继续沿用

历史 R99 初版曾把 bluetooth JSON 和 `iap2.cfg` 的 `[bluetooth]` 段一起暂存激活。较新的 `STOCK_ASI_R99_POLICY_CN.md` 已修正策略：
- `bluetooth.enableIap` 仅作为待验证的原厂配置候选；
- 不再修改 `iap2.cfg` 的 `[bluetooth]` 段；
- 原样备份并保留 `iap2.cfg`，不臆造 Wi-Fi section、MAC、ID 或凭据。

原因：`iap2.cfg` 的 Bluetooth 段目前更像 iAP2 transport 状态/报告配置，其是否是实际物理蓝牙数据通道的启动开关尚无证据。R99 prepare/verify 的离线 PASS 只证明 JSON 候选可受控生成，不能证明原厂 bluetooth 服务读取了它或 RFCOMM 已注册。

因此，R105.003 中“必须同时打开两个配置”的表述应视为待纠正的旧假设，不得据此改车机。

## 3. 当前最可信的分层数据路径

### 第一段：Bluetooth discovery / pairing / iAP2 bootstrap

`bluetooth` 与 `btstack` 是原厂蓝牙控制面。原厂 bluetooth ELF 存在 `bluetooth.enableIap`、`iapEnabled`、`SERVICETYPE_IAP2` 字符串；原厂 btstack 存在 RFCOMM/SDP 相关能力和 BlueSDK 线程候选。它们证明代码与配置标记存在，不证明 iAP2 profile/SCN 已注册。

R143 将 R92 的 btstack 进程哈希门禁、R97 的四个 pthread_create 来源跟踪和 R142 的线程名称分类合并为只读观察器。其目标线程名候选：
- `Thread_bluetoothProc`
- `bluesdk_mv8787bt_read`
- `bluesdk_mv8787bt_write`

R143 的 QEMU 合成调用点仅证明观察器逻辑能运行；不是车机真实线程、RFCOMM 服务或 iAP2 owner 证据。不要把线程名命中当成 iAP2 已连接。

### 第二段：原厂 iAP2 / AirPlay event-control data plane

较新 `inner_iap2/OEM_AIRPLAY_ABI_CN.md` 已把以下原厂 ABI 静态核验记录下来：
- 原厂 `libairplay.so` 精确 SHA256：`193a4fd9101ec2aa05e7159cfa307b96500810d379ca74a194f172adc13a46b5`；
- `AirPlayReceiverSessionSendCommand` 位于候选 VA `0x25200`，静态反汇编推导为四参数接口；
- `CFDictionarySetCString` / `CFDictionarySetData` 候选 ABI 已检查；
- 参考技术资料描述的 event/control 命令为 `type=iAPSendMessage`、`params.data=CFData`。

现有 `inner_iap2` 模块仅在同一有效 AirPlay session、库身份和 owner epoch 已被外部证明时，才转发原始 iAP2 字节；它不伪造 ACK、认证或 session ID。静态核验 ABI 和主机 socket 测试，不等于原厂 AirPlay dispatch 已将该命令接入，也不等于加密/真实 iPhone 会话已验证。

**关键缺口**：在 MU1005 原厂 AirPlay/Cinemo owner 中找到真实 event/control 收发回调、其线程和 session 生命周期，并证明第二段 iAP2 字节流确实可经该接口双向传送。

### 原厂 ASI Bluetooth iAP proxy

从用户原始资料中提取的 `libasimmxconnectivity_bluetooth_iapproxy.so` 有 9 个已识别方法：
`requestReconnectDevice`、`responseReconnectDevice`、`updateActiveDevice`、`stringRequest`、`stringResponse`、`updateActiveDevice(address)`、`updateLocalBtAddress`、`updateLocalFriendlyName`、`updateActiveDevices`。

目前没有明确识别到公开 RFCOMM 原始字节收发方法。不能把该 proxy 当作完整 iAP2 data plane，也不能因未识别到字节 API 就断言私有传输不存在。下一步需确认其工厂、消息方向、实际调用者和是否只是设备状态/重连控制。

### DIO / SI 进程边界

已静态确认原厂 `smartphone_integrator` 与 `dio_manager` 是独立 ELF/进程，不能跨进程传递对象指针。R81–R85 的 CPRX 方案已在主机真实 AF_UNIX 进程间实现 START/STOP、同会话状态采样和有线恢复确认协议；但真实 OEM 生产回调尚未绑定。主机 OEM fixtures 不等于原厂进程实际提供事件。

## 4. Wi-Fi 路线和用户要求的频段策略

`ap_live` 使用原厂 `uaputl` 对 `uap0` 做只读查询：BSS 状态、SSID、信道、AP MAC、协议和 passphrase；双次采样一致才接受，失败/超时不猜测。它不设置热点，也不直接修改无线网络。

`mu_ap_live_session` 可将真实 AP 快照交给已有 5703/4301 编码器，但尚未绑定到原厂无线连接 owner，未证明完整 iPhone 协商。

用户已经确定的目标策略必须保留：
- 同时支持 2.4GHz 和 5GHz；
- 默认优先 5GHz；
- 5GHz 不可用时自动回退 2.4GHz；
- 不牺牲原厂有线 CarPlay。

某些 donor/分支提出首次连接优先 2.4GHz；这与用户明确选择冲突，不能直接照搬。必须以 AU57x 区域/驱动真实支持情况做能力判断后执行 5GHz→2.4GHz 回退。

## 5. 统一后不再重复做的工作

1. 不再重写 R103 上游 LinkEngine；保留精确上游对象和适配测试。
2. 不再把 HFP/SIM callback 当成 iAP2；HFP=7、SIM=4 不能冒充 iAP2 profile/SCN。
3. 不再把 R143 线程名或 pthread_create 命中当成 RFCOMM 注册成功。
4. 不再把 R99 离线配置 PASS 当成运行时激活证明。
5. 不再重复实现 R81–R85 的 SI/DIO CPRX 协议；缺口是 OEM 生产回调接线。
6. 不再重做 iAP2 event/control packet parser；重点转向真实 AirPlay owner dispatch 和 transport producer。
7. 不改原厂 USB transport、`-t usbdevice`、MFi 认证函数或原厂启动器。

## 6. 下一步工作优先级（按信息增量排序）

### A. 原厂 ASI proxy 真实调用链

用已有私有原厂 ELF 及 R29 资料，建立 9 个方法的 vtable/调用者/参数对象/消息方向交叉引用，判断是否有隐藏的异步事件/transport 接口。输出逐项证据，不从方法名猜功能。

### B. 原厂 AirPlay event/control dispatch owner

从 `AirPlayReceiverSessionSendCommand` 的 caller 和接收端 dispatch 反向追踪，找到原厂 Cinemo/AirPlay owner、线程、session 创建/销毁和 iAP2 event/control handler。确认 `iAPSendMessage` 命令在目标 MU1005 上是否被处理，而不是只看第三方说明。

### C. 第一段 BlueSDK owner 和 iAP2 RFCOMM 注册

复用 R143 观察器设计，但先用不注入的 R105.004 只读诊断确认实际进程、映射模块和配置路径。之后若有必要再做经审查的只读观察。必须区分 BlueSDK 工作线程、RFCOMM owner、SDP profile/SCN 与 iAP2 LinkEngine 的状态边界。

### D. 双段联调与有线抢占

只有 A/B/C 的真实 owner 都有证据后，才连接第一段蓝牙 bootstrap 与第二段 AirPlay event/control data plane；使用既有 CPRX 进程协议和回滚屏障。然后实现连接重试、掉线清理、USB 有线抢占、iPhone 切换。

### E. 安装包门禁

最终才做中文 SD 自动备份、安装、校验、回滚。必须同时满足 QNX ARM 目标构建、真实 owner/认证/媒体链路和有线恢复验证。当前任何研发包都不准装车。

## 7. 当前真实进度

| 子系统 | 源码/静态研究 | 真实 OEM 运行 | 真实 iPhone/MFi |
|---|---|---|---|
| BlueSDK 线程观察 | 有 R142/R143 只读实现 | 未在 OEM btstack 验证 | 未验证 |
| iAP2 LinkEngine | 已复用上游并有 host tests | 未接入 OEM RFCOMM owner | 未验证 |
| ASI Bluetooth proxy | 9 个方法静态识别 | 实际调用链未闭环 | 未验证 |
| SI 原厂注册/激活 | 有静态偏移和受控 adapter | 未证明 owner callback | 未验证 |
| DIO/SI CPRX | 主机跨进程协议与回滚状态机 | OEM 生产回调未绑定 | 未验证 |
| AirPlay 第二段 iAP2 | ABI 静态分析与 socket relay 源码 | dispatch hook 未定位 | 未验证 |
| Wi-Fi AP | 原厂 uaputl 只读采样源码 | 尚未执行真实读取 | 未验证 |
| 5GHz/2.4GHz | 策略和编码接口 | 频段切换未验证 | 未验证 |
| 最终安装 | 无可安装包 | 未安装 | 未验证 |

**总状态：有多个可复用的源代码模块和静态证据，但无线 CarPlay 尚未实现。下一轮不再泛泛搜配置；优先完成 ASI proxy 的真实调用链与 AirPlay event/control dispatch owner 两条静态逆向，再用一次只读实车快照确认实际运行路径。**

`installable=false` / `safe_to_flash=false` / `vehicle_verified=false`。
