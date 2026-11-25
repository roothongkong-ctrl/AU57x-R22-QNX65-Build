# MU1005 原厂连接管理回调注册与分发实证（NO INSTALL）
输入：用户上传 app.img/eso/bin/apps/btstack，1846436 bytes，SHA256 51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566。使用实际 ELF 的 readelf、llvm-objdump 和 ARM direct branch scan，未执行原厂程序。

## 注册链
- 0x152d14 构造候选：保存第二参数到 wrapper+0x88；初始化前3个字段；0x152d7c 读取 GOT offset 0xe38，0x152d84 从 GOT 0x2c2060 取原厂 CmgrHandlerEx::_cmgr_handler_ex_callback（0x152718）；0x152d88 调用 0x243574，r0=wrapper+12、r1=callback。
- 0x243574 检查参数；0x243598 调用 0x27a12c，其 PLT 0x1170c0 已由动态符号表确认是 pthread_mutex_lock。原厂注册初始化124字节 handler，0x2435fc 保存回调至 handler+8；0x24362c 插入原厂列表；0x2435bc 调用 0x27a0a0，其 PLT 0x1178dc 为 pthread_mutex_unlock。
- 0x152d14 的直接调用候选为 0x13c82c、0x13f8f8、0x14f0a0。0x13f8f8 前分配140字节 wrapper，传 job+0x24 的回调对象；对应 BtJobStartService 路径。不是全局 iAP2 初始化。

## 分发与撤销
- 0x24232c 遍历 SDK handler 列表，匹配设备对象和启用标志。0x2423c8 从 handler+8 读取回调，0x2423d0 BLX，三个参数为 handler、事件、状态。直接调用候选 0x244150、0x2442cc、0x244324、0x2443b4、0x2444b8、0x2449dc；另有 0x243fc0 间接调用路径，不能把单个分发器当作全部入口。
- 0x152718 回调检查原厂 active handler list（CmgrHandlerEx::s_ActiveHandlers），匹配 wrapper+12；0x15285c 读取 wrapper+0x88，0x152884 通过其 vtable 执行 job 回调。原厂事件码及状态参与后续清理；未把该事件声明为 iAP2 认证或全局 ACL。
- 0x152c2c 析构候选通过 0x152c84 调 0x243248 注销 handler；后者有同一 mutex lock/unlock 包裹列表查找、清理和移除。尚需完整核对注册失败及回调重入时序。

## 结论
已闭合原厂连接管理注册及回调间接调用的静态链路。注册和注销存在真实 mutex，不表示事件分发持有相同锁，也不能证明回调允许重入 SDK 或注册 RFCOMM。线程源与启动/停止时序仍是阻塞。禁止仅依据上述地址做运行时 hook、调用或替换 HFP/SIM。保持原厂 MFi、有线 CarPlay 和 NO INSTALL。仅提交地址和分析文字，不上传原厂 ELF。
