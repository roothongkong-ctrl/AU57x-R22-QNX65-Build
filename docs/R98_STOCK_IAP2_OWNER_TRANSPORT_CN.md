# R98：原厂 iAP2 owner → 本机 57342/TCP → R45 native 注册函数（源码与离线回归）

**基线**：2017 A6L C7 PA、中国版 MHI2_CN_AU57x_K1006_2/MU1005、QNX 6.5 ARMv7。

## 这轮真正新增了什么

- `bt_iap2_owner_handoff.[ch]` 是位于原厂进程一侧的 **具体接入调用层**，不是 R97 的 HFP 观察器。它提供真实的非阻塞本机 TCP `connect(127.0.0.1:57342)`（有界超时、返回 fd、CLOEXEC，核验双方地址），连接 R44/R45 原有的本机收发入口。
- 与 `native_binding.c` 之间新增 QNX ARM32 的 `au98_bind_stock_native()` **实际函数转发路径**：`au98_bind_stock_native` → `au98_publish` → `mu_bt_native_attach` → R45 的 `ir5_init/ir5_publish` → 原厂 BlueSDK RF/SDP API。后续 `au98_tick` 与 `au98_withdraw` 直接使用 `mu_bt_native_tick/withdraw`。宿主机不能冒充 QNX 原厂绑定，默认直接拒绝这条 QNX 函数。
- 在接入前要求**分别**确认：原版 `btstack` 程序已校验、当前线程就是原厂 **iAP2** 串行回调 owner（不是 HFP 回调线程）、该服务确实是 iAP2 所有者、原有 `MuBtOwner` 的本机身份与传输/配对身份。注册参数沿用 R91/R45 的 `mu_rf_arg_choose`；QNX 快速实验配置可以尝试参数 0，但不伪称它已确认。
- 同一会话只允许注册一次。注册返回失败后可能已经写入原厂 SDP/RF 对象，因此状态进入 `POISONED`、不重新注册或释放可能被原厂持有的内存/套接字。正常撤销必须由原厂 owner 与独立的 callback barrier 确认。
- `tests/test_r98_iap2_owner_handoff.c` 真正启动本机 TCP listener + socket connect + accept，验证独立授权、同进程服务角色、真实 fd、错线程、重复会话、成功撤销、注册中途失败的不可重用状态。**fixture 是虚构的原厂 owner 证明，不是汽车**。

## 为什么仍不能安装

1. 目前**没有原厂 MU1005 iAP2 线程/服务 owner 的真实证明函数**。R97 只核对 HFP/SIM 事件入口，不能用 HFP 代替苹果 iAP2 回调。
2. 原厂 `RF_RegisterServerChannel` 的 iAP2 profile 参数仍未知；R91 实验 `0` 是候选值，HFP/SIM 真实 7/4 不能照搬。
3. 目前没有证明已经在实车 `btstack` 实际执行此 `au98_bind_stock_native` 路径，也没有 iPhone 原厂 MFi/无线 Wi-Fi/AirPlay 成功证据。
4. `LD_PRELOAD` 和启动配置都是源码/离线候选；原厂 `connectivity.json` 的实物尚未确认。

**本轮成绩**：R97 QNX ARMv7 编译、ELF 与 QEMU 检查已确认通过；R98 新增真实 TCP/C owner 交接与 QNX native API 绑定的源码，43/43 主机 C 编译通过，新的 Socket gate 和失败回滚实验通过。QNX 真正运行 native 路径不在 QEMU 空载测试覆盖内。继续采用用户指定的“先全部离线完成，最后上车一次”流程。

**installable=false / vehicle_verified=false / mfi_apple_verified=false / NO_INSTALL。**
