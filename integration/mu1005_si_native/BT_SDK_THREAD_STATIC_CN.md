# MU1005 SDK 原始线程及底层连接事件注册（NO INSTALL）
2026-11-25；原始 btstack 1846436 字节，实际 sha256sum = 51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566。通过本地 llvm-objdump、readelf 和 PT_LOAD ARM 直接分支/字面量扫描；没有执行 ELF。

## 已核实的线程创建
pthread_create PLT 0x117390，直接 BL 候选 0x236668、0x2368c8、0x236c2c、0x236c48。此扫描不覆盖间接调用或其他 C++ 线程包装。
0x236b48 函数 GOT 基址由 0x236b58 的 PC-relative ADD 得到 0x2c1228。0x236c2c 的 r2 来源 literal 0x236dd8 + GOT base = 0x2375b8；0x236c48 的 r2 来源 0x236de0 + GOT base = 0x23705c。后续 0x236c58 / 0x236c68 调用 pthread_setname_np（PLT 0x117834）；对应原厂名字分别为 bluesdk_mv8787bt_read / bluesdk_mv8787bt_write。0x236a60、0x236a70 为两次 pthread_join，具体线程对象关联需继续验证。0x2368c8 的入口候选为 0x234ca8，函数循环调用 0x234548 和 0x116e14；尚未认定其角色。0x236668 从 GOT relocation 取 HC_WndProc（0x24086c），线程具体语义未认定。
读线程 0x2375b8 有独立 mutex/cond_wait；0x23769c 调 0x27a568，后者调用 pthread_mutex_lock；0x2376e8 调 0x27a4dc，后者调用 pthread_mutex_unlock；两者均调用 0x279f80 并使用返回对象+12 的锁指针。未闭合该对象与 RFCOMM 注册锁关系。不能将读写线程当作 iAP2 owner。

## 连接管理内部事件入口
0x243640 初始化 cmgrContext（148 字节，GLOB_DAT 0x2c20bc -> 0x494b40）。0x243668 从 literal 0x2436f8 读取 GOT-relative offset，0x243670 计算为 0x243d6c，0x243674 写 context+8；0x243678 调用 0x1db97c。
0x1db97c 检查输入及 callback+8 非空；0x1db9b8 读取 GOT 0x2c1964（符号 bt，VA 0x2d58d4），0x1db9c0 调列表查询；0x1db9d0/0x1db9e0 为已确认 mutex 包装，0x1db9dc 插入列表。此为底层事件订阅候选，而非 iAP2 RFCOMM 注册。
0x243d6c 是独立函数入口（保存寄存器、PIC 基址、事件码跳转表）。其中 0x243fc0 为间接 handler 调用；0x244150、0x2442cc、0x244324、0x2443b4、0x2444b8、0x2449dc 调用 0x24232c。另有 0x243330 将同一 0x243d6c 存到 handler+0x20，不能假设只有一个事件来源。
前轮只确认连接回调注册，今轮进一步确认其下层订阅链。仍缺 bt 列表实际分发点到 pthread 入口的完整路径、SDK 锁所有权及注册/撤销时机。不能在此候选位置直接加载/调用/打补丁；不能认定真实 iAP2 或 MFi 已成功。原厂 ELF 不提交，保留有线，不安装。
