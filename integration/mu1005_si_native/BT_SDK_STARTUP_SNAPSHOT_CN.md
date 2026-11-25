# MU1005 原厂 SDK 实际启动与事件队列（NO INSTALL）
2026-11-25，实际用户 btstack 1846436 bytes，SHA256 51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566；独立 36 项静态核验全部通过，原始文件未上传。
- 原厂 startBtStack 0x11eab0 调 OSAPI start 0x27b190；0x11eab4 未检查 r0 直接调用下一函数，外层 0x11ec98 可置 this+0x1b0=1，故外层标志单独不足。
- 0x27b194 获取 OSAPI 单例（GOT-relative 0x27a084 -> 0x2d5644），0x27b198 调 0x27a660；后者在 0x27a6a0/ac/b8 检查三个 mutex、0x27a700 保存 timer、0x27a708 调底层 init，0x27a710 根据返回分支。成功 0x27a7dc 写 OSAPI+0x16=1；失败走 0x27a614 清理。0x27a644/648/64c 清除 started、timer、callback。0x279ee8/0x279f14/0x279f40 使用原厂 MutexOps::init；三者分别保存到 OSAPI+4/8/12。
- SDK notify 的队列：0x27a1b8 -> 0x27a300 调 comm::ActiveObject::enque；GOT 0x2c1760 的 R_ARM_GLOB_DAT 指向 DelayedCallbackEvent<bool> vtable 0x2bee20，槽 0x2bee30 -> 0x14d4cc，后者通过回调对象 vtable+12 间接调用。OSAPI::notifyCallback 0x278448 -> 0x1e47cc。后者 0x1e47e0 持 SDK+4 锁，0x1e47e4 持 SDK+8 锁，0x1e4828 放 +8，0x1e4834 执行事件，0x1e4838 重取 +8；0x1e4b64 放 +4。不要假设整个分发无锁，更不能直接嵌套原厂注册。
- 新源码 bt_startup_snapshot 用已认证 owner 传入的真实 BluetoothServices this 及检查过的 read_live，双读对象 vptr、外层 started、OSAPI vptr、内部 started、三个 mutex 和 timer 指针；拒绝变化、读取错误、外层假启动和错误对象。接入 native_attach/ready/register/tick，保留单独的 cleanup owner 判定。测试仅 mock read；不是运行线程证据，不安装。
- 仍缺真正的 owner provider、ActiveObject 外部库执行线程和锁上下文、原厂注册与撤销证据。既有真实 QNX socket 测试不替代这些。保持原厂 MFi、有线、NO INSTALL。
