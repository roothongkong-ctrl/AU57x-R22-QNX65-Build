# R142：MU1005 原厂蓝牙线程名观察（QNX 6.5 ARM 验证版）

## 目标与边界
本版本仅观察 pthread_setname_np() 调用并透明转发原函数。它不调用 RFCOMM/SDP/iAP2/MFi/AirPlay，不改写 OEM 内存、配置或设备节点，也不代表原厂 BlueSDK owner 已确认。

## 参考库分析
参考 libbtwcp.so 的 pthread_setname_np 处理方式：从线程名第 2 个字节开始，大小写敏感地查找 bluetoothProc；匹配后将该线程分类为主候选。mv8787bt_read/write 只作为次级 BlueSDK 线程观察对象。

## MU1005 依据
- 原厂 btstack SHA256：51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566
- 实车只读线程名证据：Thread_bluetoothProc、bluesdk_mv8787bt_read、bluesdk_mv8787bt_write
- 本轮不携带或修改原厂二进制。

## 验证层级
1. Host GCC/Clang fixture：仅验证代码逻辑。
2. QNX 6.5 ARM 编译：验证目标 ABI、动态符号和 libc 依赖。
3. QNX QEMU LD_PRELOAD smoke：验证目标动态加载器、构造器、RTLD_NEXT 转发和线程名分类。
4. OEM btstack 进程：本轮不运行真实原厂进程，尚待独立验证。

QNX QEMU smoke 使用合成测试程序，不包含车机蓝牙硬件，也不证明真实 OEM btstack 能启动或 iAP2 owner 已绑定。

## 运行
- Host：python3 tests/test_r142_thread_name_probe.py
- QNX build：bash ci/build_r142_qnx.sh
- QNX QEMU 由 GitHub Actions 使用固定 QNX 6.5 ARM 工具链运行。

## 安全
R142_REVIEW_ONLY=1 为强制编译门禁；默认关闭观察；不安装、不刷写、不改变车机文件。
oem_executed=false；vehicle_verified=false；native_iap2_owner=false；mfi_verified=false；wireless_carplay_verified=false；installable=false；safe_to_flash=false。
