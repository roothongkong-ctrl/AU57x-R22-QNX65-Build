# R43 — 移植 Luka 的 iAP2 链路层，接入 MU1005 RFCOMM（源码审查版）

**上游来源：** `luka-dev/mib2q-carplay-rgi/wireless/daemon/vendor/iap2-link/src/lib.rs`，GPL-3.0。这里是为 QNX 6.5 ARMv7 C 项目改写的 **受限 C99 移植**，不是 Rust 完整链接库；已保留标记探测、链路同步参数、控制会话10、分段接收、ACK、重传和超时，以及对原厂所有者/连接代际的失败关闭策略。

- `cp_step04_link.[ch]` 使用既有 `AuRfWire` 规范化蓝牙队列作为唯一入口/出口，使用 `au_rf_daemon_data` 深拷贝到SDK待发送缓冲；原厂原始SDK函数仍由车机合法owner负责，**未注册或hook真实BlueSDK**。
- START 的 `FF 55 02 00 EE 10`、SYN/LSP、`FF 5A` 链路帧、控制会话10与 `40 40` 控制消息分开处理。拒绝错误校验和、超长帧、错误代际和无可信owner；重传保持PSN与应用层令牌不变。
- `cp_lk_queue_control` 可由 R42 `cp_auth_owner.send_csm` 调用，已接入 RFCOMM 发送管线；`on_control` / `on_acked` 回调可承载 R42 `cp_auth_receive` / `cp_auth_tx_confirm`。此版本先验证链路/蓝牙接线，下一版本将实现受证明owner的认证回调适配。
- 使用固定容量队列和1个未ACK的控制消息。尚未覆盖完整原版Rust窗口、乱序补包、外部附件会话11/12或长期高负载；绝不能把此子集标为完整 iAP2 实现。
- 宿主 ASan/UBSan 检查字节流分段、SYN/ACK、旧PSN重复、应用token确认、异步RFCOMM发送队列及原厂owner撤销；QNX CI检查ARMv7构建和QEMU导出符号。

**未完成：** 原厂MU1005 BlueSDK RFCOMM实际注册/回调owner、原厂MM-IPOD传输选择、真机合法MFi证书验证成功、iPhone真实会话，仍全部 NO_GO。不替换固件，不提供可安装载荷。
