# R105.009｜原厂 iAP2 owner 非阻塞接入边界修正

日期：2026-11-26
基线：MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
状态：SOURCE_FIX + HOST_GATE / NO INSTALL / vehicle_verified=false

## 本轮发现的真实源码风险

复核 R98 原厂接入源码时发现：旧的 `au98_bind_stock_native()` 会调用 `au98_publish()`。而 `au98_publish()` 内部会执行 loopback TCP connect，并通过 poll 等待最多约 1 秒。虽然注释已声明它属于 worker-side bounded connect、不能在 SDK event dispatch 上调用，但函数名是 stock native bind，容易被错误地从原厂 owner callback 直接调用。

对于 BlueSDK/iAP2 串行 owner，这是不合适的：SDK callback 不应承担 socket 建连、poll 等待或网络调度；可能阻塞原厂协议事件处理。

## 已实施修正

1. 将 `au98_bind_stock_native()` 保留为兼容符号，但改为 fail-closed：直接返回 -1，不创建 socket、不 poll、不触碰 BlueSDK。
2. 唯一允许的原生接入路线收敛为：
   - 独立 worker 线程准备 loopback TCP fd；
   - fd 必须 nonblocking + CLOEXEC；
   - 通过独立 daemon identity witness 核验 endpoint；
   - 将 fd 转交原厂 iAP2 owner；
   - owner 线程通过 `au98_drive_stock_native()` → `au98_drive()` → `adopt_dispatch()` → `au98_adopt_stock_native()` 接管；
   - attach、tick、withdraw 仍只在已证明的原厂 iAP2 owner/安全 dispatch 上执行。
3. 新增 `tests/test_r105_owner_nonblocking_gate.py`，对源代码结构做静态 fail-closed 门禁，确保：
   - legacy bind 不再调用 publish/connect/poll；
   - native drive 使用 prepared-fd adopt 路径；
   - adopt dispatch 不创建 socket；
   - blocking connect 仅属于 worker-side API；
   - owner 线程合同明确排除 HFP。
4. 将该门禁加入 R105 GitHub Actions；后续修改若意外把阻塞建连放回 owner path，会自动失败。

## 验收范围

该修正消除的是代码层“可能在原厂 owner callback 中阻塞建连”的风险。新测试是源代码结构检查，不等于真实 BlueSDK owner callback 已经找到，也不证明 daemon identity witness 已经由车机运行时提供。

当前仍缺：
- MU1005 真正 iAP2 owner 的可执行接入点；
- worker 端真实 daemon 身份证明；
- 原厂 BlueSDK 注册参数与 iAP2 SCN 的完整证明；
- 真实 RFCOMM、Apple MFi、AirPlay/Wi-Fi、媒体链路和 iPhone 连接验收。

**本轮结论：封住一条不应在 SDK owner 线程运行的阻塞接入路径；后续只允许 worker 预建连接、owner 非阻塞接管。**

installable=false / safe_to_flash=false / vehicle_verified=false / mfi_apple_verified=false。


## 自动门禁实测结果（2026-11-26）

当前分支专用 R105 workflow run `45756262630` 已完成 SUCCESS：
- 上游 iAP2 LinkEngine 原始文件 hash 校验与 Rust 测试通过；
- AU57x C frame 测试通过；
- 新增 5 项 owner nonblocking 源码边界测试全部 PASS；
- workflow scope artifact 明确记录 HOST_ONLY、OEM owner NOT PROVEN、MFi NOT TESTED、VEHICLE_VERIFIED=0、INSTALLABLE=0。

这次成功是主机回归门禁成功，不是车机 owner 已接通。
