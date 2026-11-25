# 集中真实 QNX 运行验收（NO INSTALL）
本轮把独立网络 worker、原厂优先 notify gate、Au98 生命周期整合到一个真实 socket/pthread 测试。使用真实 57342 TCP listener、worker 准备两次 fd、owner 线程模拟原厂通知、wired 优先撤销、独立回收门、第二代连接及最终关闭；没有调用固定 OEM 地址。主机与 QNX ARM 各执行一次完整场景，QNX 使用既有真实 io-pkt-v4-hc 启动环境。SDK 回调/证明和定时 pulse 都明确为 fixture；不宣称真正的原厂启动、MFi 或 iPhone。
原厂本轮重新反汇编 0x27a1b8：取 OSAPI+0x3c、构造 16 字节 DelayedCallback、持有 OSAPI+0x28 callback 引用、enque 0x117900；没有可供调用方观察的 enqueue 成功返回值。原厂 0x1e4a8c/94 和 0x1e4d70/80 以 bt+0x1bf258 作为通知合并标志后调用该函数。直接从 worker 周期调用会绕开原厂合并与停止生命周期，且无法用普通 notify 确认某次特定队列事件完成；因此不将此地址直接当跨线程周期唤醒接口。下一步需要安全周期事件的真实 owner 调度调用方；保留原厂 MFi、有线优先、不刷写。
上一轮 host 45561902008 成功，最新 ARM/cache 45561902017 尚在进行。新 QNX 测试需等待实际 Actions 结果。
