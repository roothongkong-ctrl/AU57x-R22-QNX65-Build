# R44：单权属链路适配：RFCOMM → iAP2 链路 → 原厂 MFi CONTROL 认证

在 R43 Luka iAP2 链路移植之上新增 `cp_step04_session.[ch]`，它将 R39/40 `AuRfWire` 蓝牙队列和 R42 `cp_auth_receive/tx_confirm` 使用同一 owner / epoch / generation 串接。

- 真实 iAP2 数据帧通过 CONTROL session 10 送入认证回调，不再从测试中直接调用一个完全独立的 MFi 状态机；AA00 读取原厂 API（主机测试中为模拟版本），AA02 读取挑战签名，待对端正确 ACK 才完成发送确认，AA05 仍依赖原厂可信 `peer_auth_succeeded` 证明。
- 蓝牙owner与控制owner必须分别提供真实、同代际的证据；有线CarPlay接管后立即拒绝；非 AA 认证控制帧需要原厂所有者的独立处理回执，否则硬失败。
- `tests/test_step04_session.c` 使用合法结构的模拟手机 iAP2 字节流 + 主机 MFi 假函数验证端到端软件路径。

**安全边界：** 测试桩不是实际 Apple MFi，原厂 `libairplay.so` 的运行时 handle/ABI 仍待核实；蓝牙SDK callback owner、设备端 `iap`/`mm-ipod` 进程路径仍未证明。QNX 审查库通过编译/QEMU符号检查不等于无线CarPlay真实可用，禁止装车。
