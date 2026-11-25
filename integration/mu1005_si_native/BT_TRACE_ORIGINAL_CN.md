# MU1005 原始 btstack ACL 入口静态实证（NO INSTALL）
2026-11-25，用户重新提供 app.img 内 eso/bin/apps/btstack。1846436 bytes；SHA256 51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566，匹配固定原厂版本。ELF32 ARM EABI5 ET_EXEC，解释器 /usr/lib/ldqnx.so.2。原始文件只在私有分析环境，未提交。
使用 readelf、llvm-objdump 和 Python 读取实际 ELF；不是合成 fixture。原厂 PIC/GOT 基址 0x2c1228；0x13f390 的 PC-relative ADD 和 literal 0x13f7fc 复核得到该基址。上轮直接引用定位器没有覆盖这种 PIC/GOT 路径。

| 观察 | 字符串 VA | literal load | 字符串地址形成 | trace 调用 |
|---|---|---|---|---|
| ACL established | 0x285d44 | 0x13f6ac | 0x13f6bc | 0x13f6c4 |
| ACL lost | 0x285d88 | 0x13f544 | 0x13f554 | 0x13f55c |
| ACL already connected | 0x285bb0 | 0x13f1c0 | 0x13f1d0 | 0x13f1d8 |

实际函数入口候选 0x13f37c：BtJobStartService vtable 符号 VA 0x2bec90，最后一项 VA 0x2becc4 内容为 0x13f37c，且原厂 GOT relocation 0x2c19ec 指向该 vtable。该函数将事件地址与当前 job 的地址进行比较，检查事件码和状态后才进入 established/lost 分支；不是全局 ACL 监听器。另一个原厂导出 CmgrHandlerEx::_cmgr_handler_ex_callback 位于 0x152718，GOT relocation 0x2c2060；包含匹配 handler、间接回调及清理分支。两者的完整调用链、线程和锁上下文仍需独立验证；不认定为 iAP2 注册线程，绝不直接调用或修改这些地址。

Device ID SDP 失败日志两个静态引用位于 0x26b098、0x26b140（不是已经找到初始化完成事件）。下一步追踪实际 callback/SDK dispatcher 及其注册与撤销生命周期，而不是把日志位置当作 hook 或认证证据。MFi、真实 iPhone、媒体输出、安装均未验证。installable=false。
