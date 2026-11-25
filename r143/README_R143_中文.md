# R143：MU1005 原厂蓝牙进程联合观察器（仅研发，不安装）

## 本轮目标
把 R92 的原厂进程 SHA256 门禁、R97 的四处 pthread_create 来源跟踪、R142 的线程名称识别合并成一个只读预加载模块。

目标固件：MHI2_CN_AU57x_K1006_2 / MU1005，QNX 6.5 ARM32。
原厂 btstack SHA256：51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566。

## 观察内容
- 进程 basename 必须是 btstack。
- 原厂 btstack 文件 SHA256 必须精确匹配。
- 只对原厂四处已核验 pthread_create 调用点建立线程来源跟踪。
- 透明转发 pthread_create 和 pthread_setname_np 的参数与返回值。
- 记录 Thread_bluetoothProc、bluesdk_mv8787bt_read、bluesdk_mv8787bt_write 的线程名，并判断目标线程是否来自已跟踪的原厂调用点。
- 观察默认关闭；不调用 BlueSDK、不注册 RFCOMM、不改 GOT、不触发 HFP/iAP2/MFi/AirPlay。

## 验证方式
1. GCC / Clang 主机测试：用合成 btstack 进程检查精确哈希门禁、错误哈希拒绝、线程来源与名称关联。
2. QNX 6.5 ARM 编译：分别生成合成 QEMU 测试库和真实 MU1005 哈希/路径固定的 ELF 审查库。
3. QEMU：只运行名为 btstack 的合成程序；通过 /proc/boot/btstack 的 SHA256 门禁；用测试 API 模拟已核验的原厂调用点，检查线程名称与 owner 跟踪关联。
4. 真实 OEM btstack：本轮不执行，不代表原厂进程已加载。

## 重要边界
QEMU 中的 stock callsite 是测试 API 传入的合成地址，不是真实 OEM 指令正在执行。QEMU 通过只证明 QNX 动态加载、哈希门禁、线程包装和名称关联代码可运行。它不证明车机监督进程会保留 LD_PRELOAD，不证明真实 BlueSDK 事件 owner，不证明 iAP2 RFCOMM、Apple MFi 或无线 AirPlay。
真实 HFP 与 iAP2 是不同服务；本模块不安装 HFP GOT 观察器，也不修改任何 OEM GOT。

状态：host_tests=PASS（待 CI 复核）；qnx_arm_build=PENDING；qnx_qemu=PENDING；oem_btstack_executed=false；native_iap2_owner=false；mfi_verified=false；wireless_carplay_verified=false；installable=false；safe_to_flash=false。
