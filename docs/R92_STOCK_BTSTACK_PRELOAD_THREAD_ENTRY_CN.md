# R92 — MU1005 原厂 btstack 的真实进程加载入口（不等于 iPhone 已连接）

日期：2026-11-22。目标固件：`MHI2_CN_AU57x_K1006_2 / MU1005`；原厂 `btstack` SHA-256：`51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566`。

## 这一轮真正做了什么

此前 R91 只能从一个不存在的“原厂 owner 回调”进入 `mu_bt_native_attach`，即使 RFCOMM 实验值 `0` 已允许，也没有实际 QNX 动态入口。R92 新增**真实可加载的共享库源码** `bt_preload_probe.c`，用 QNX Neutrino 6.5 官方支持的 `LD_PRELOAD` 机制进入进程。原厂 `btstack` ELF 文件不修改，默认关闭探测。启用后：

1. 仅在 `AU57X_R92_PROBE=1` 时启动；读取目标 `/eso/bin/apps/btstack` 的全文件 SHA-256，必须与以前用户提供的 MU1005 ELF 完全一致。仅主机测试版允许改目标路径。
2. 构造器验证通过后以 `O_APPEND` 将 `stock-image-pinned` 写入 `/tmp/au57x_r92_bt_preload.log`。
3. `LD_PRELOAD` 符号插桩 `pthread_create`：从 `RTLD_NEXT` 取得原函数，**调用参数、返回结果和线程入口保持不变**，只记录调用地址、线程入口和是否命中原厂的四处已知调用点；不自动启动任何额外线程。
4. 命中的原厂 `BL pthread_create` 指令地址为 `0x236668`、`0x2368C8`、`0x236C2C`、`0x236C48`，PLT 入口为 `0x117390`（来源：用户自己的 1.8 MB ARM QNX `btstack`，经 LLVM 反汇编核对）。审计结果在 `R92_MU1005_BT_PRELOAD_AUDIT.json`。
5. GitHub 云端工具链另外构建 `libau57x_bt_preload_qnx_REVIEW_ONLY.so`，其依赖为 QNX libc，供**离线**检查。仍保留 R91 原始 MFi 和 RF 参数 `0` 实验分支。

本轮的函数包装属于“真正的进程入口接线”，但**还不是**“原厂蓝牙事件回调入口”。仅有这 4 个线程起点不足以知道哪个是 BlueSDK 事件 owner。下一阶段应从真正启动的 `btstack` 记录里确定 owner 线程与 SDK 初始化次序，再把 R91 的 `mu_bt_native_attach` 放到原厂 owner 上调用。不能把 Linux 的测试线程说成车上线程。

## QNX 6.5 参考

官方参考：

- https://qnx.com/developers/docs/6.5.0SP1.update/com.qnx.doc.neutrino_utilities/envars.html（LD_PRELOAD）
- https://get.qnx.com/developers/docs/6.4.1/neutrino/lib_ref/d/dlsym.html（RTLD_NEXT）

原厂程序如果设有 setuid，QNX 会忽略 `LD_PRELOAD`。原厂监督进程可能清除环境变量或覆盖子进程启动参数；是否能由原车启动流程传入 `LD_PRELOAD` 仍需实车核验。原厂自有符号优先级/PLT 解析也必须实际观察。

## 主机复现

```bash
bash ci/host_bt_preload_probe.sh
python3 -m unittest discover -s tests -p test_mu1005_bt_preload_audit.py -v
python3 tools/mu1005_bt_preload_audit.py --btstack /private/MU1005/btstack --out /tmp/audit.json
```

主机脚本实际运行一次用 GCC 链接的模拟 `btstack` ELF，并通过 `LD_PRELOAD` 注入真实共享库，验证 `pthread_create` 转发两次；反例包括错误哈希和关闭探测。这不是假冒原厂服务。

## 车辆下一关的事实边界

**本包没有车机自动启动器，没有修改原厂监督程序，也没有在车上加载库。** 进程启动前必须保证不会启动第二个 `btstack`。需要稳定供电、可独立恢复的原厂启动脚本以及车上日志采集。**不能把 R92 ZIP 直接解压 SD 卡就当成已完成无线 CarPlay。**

已确认：本地真实 ELF 装载机制与 pthread_create 包装；原厂四处线程调用点静态匹配；之前 R91 的 C/IPC/iAP2/MFi 测试链。

未确认：车机进程 `LD_PRELOAD` 能否继承、BlueSDK 事件所有者、R91 RF 参数零的实际结果、手机配对后的 iAP2、MFi、AirPlay、车辆效果。

`installable=false`，`vehicle_verified=false`，`safe_to_flash=false`，`NO_INSTALL`。
