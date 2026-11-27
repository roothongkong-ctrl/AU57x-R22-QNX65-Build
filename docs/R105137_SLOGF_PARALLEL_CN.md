# R105.137 四路共享环境提速（NO INSTALL）
R105.135 SI 和 R105.136 btstack 均已越过 libecpp-ne.so.4 缺库，真实运行报告共同缺少 slogf；旧 R105.104 已确认原厂 liblogging.so.1 导出、R105.106 已确认 SDK libc.a 定义。不重新扫描旧报告。一次解包、一次工具链构建，SI/BTStack × 原厂日志库/SDK 实际静态归档实现四路独立 QEMU guest 并行。严格校验原厂候选哈希、实际导出、依赖唯一性；SDK 共享链接若失败则该路阻塞，不造假函数。不改车机或认证。已有 Bluetooth/iAP2 任务保持运行。四路是同一 runner 内的并行实验，不等于四台 runner。测试完成才判断实际修复。
