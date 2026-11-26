# 固定交付清单第01步 — RFCOMM双向通信（仍未验收）
基线 R105.040 e3075668c88248e431c2c339c8fd29bf36b8168b，三个云端 workflow success。
复用 R45/R5 native_binding、native_event、RF ingress、IPC stream、R98 owner handoff、OSAPI queue pulse，不另造传输协议。现行原厂代码将 RF 注册参数解释为 RX credit 1..7，MU1005 allocator slot 1..6；旧 R101 文档的通用 SCN 1..30 不是该私有 allocator 合同，不更改现行范围。
本次修复 mu1005_socket_pump.c：TCP recv 原为固定 2048 字节，可能在原厂异步 TX 持有 descriptor 时一次读取超过8条最小 DATA 记录，使 au_ipc_feed 队列满并断开。按剩余槽数与已接收的 partial frame 计算安全读额度，无空位不 poll POLLIN。复用 TCP 自然背压，不丢帧，不伪造 event6。r14 实际 loopback 测试新增12条合并记录，模拟真实原厂完成顺序，验证全部字节有序传递。
仍缺：MU1005 原厂生产 owner 的启动/撤销绑定、真实配对授权与 RFCOMM 注册、真实 iPhone 双向字节证据。上述源码及 host fixture 不等于生产接通。第01步未划掉；不安装、不绕过 MFi。
