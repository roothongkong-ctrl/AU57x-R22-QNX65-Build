# 固定交付清单第01步 — RFCOMM双向通信（仍未验收）
基线 R105.040 e3075668c88248e431c2c339c8fd29bf36b8168b，三个云端 workflow success。
复用 R45/R5 native_binding、native_event、RF ingress、IPC stream、R98 owner handoff、OSAPI queue pulse，不另造传输协议。现行原厂代码将 RF 注册参数解释为 RX credit 1..7，MU1005 allocator slot 1..6；旧 R101 文档的通用 SCN 1..30 不是该私有 allocator 合同，不更改现行范围。
本次修复 mu1005_socket_pump.c：TCP recv 原为固定 2048 字节，可能在原厂异步 TX 持有 descriptor 时一次读取超过8条最小 DATA 记录，使 au_ipc_feed 队列满并断开。按剩余槽数与已接收的 partial frame 计算安全读额度，无空位不 poll POLLIN。复用 TCP 自然背压，不丢帧，不伪造 event6。r14 实际 loopback 测试新增12条合并记录，模拟真实原厂完成顺序，验证全部字节有序传递。
仍缺：MU1005 原厂生产 owner 的启动/撤销绑定、真实配对授权与 RFCOMM 注册、真实 iPhone 双向字节证据。上述源码及 host fixture 不等于生产接通。第01步未划掉；不安装、不绕过 MFi。

## 第01步实际验收核对（继续工作，不新增主步骤）
- R105.041 CI run 45860679657 所有9个 jobs success；native-library job 140050573975 success；transport job 140050573942 日志两次明确 PASS TCP burst > RF queue capacity，R5 publication、EOF close/completion 两种顺序均 PASS。SI run 45860679560 success。故此前新增双向环回传输测试已验证；不是实车蓝牙测试。
- 新增经典 MHI2 参考（不是重复 MU1316/QC）：omonob/VW-MHI2-Wireless-Carplay-Maps commit aec37f5b1c87a0d961d16fe4328ac3638d0a833e。实际读取该版本 native-baselines.tsv、btstack_wrap.sh、WIRELESS_DEVELOPMENT.zh-CN.md 及 libbtwcp.so ELF。公开基线仅 MU1102/MU1367/MU1440，三组哈希均不等于本目标原厂 btstack SHA256 51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566；不复制 donor 库或硬编码地址。公开包没有独立完整实现源码。
- donor libbtwcp.so 实际 base64 解码 97676 bytes，ELF32 ARM ET_DYN，.dynsym 导出 pthread_setname_np @0x13770 size536, pthread_cond_wait @0x13a78 size232, pthread_cond_timedwait @0x13d34 size240；包含 RFCOMM/SDP/credit 诊断字符串。只能据此确认 interposition 接口，不能根据符号就断言锁合同/具体绑定时机。donor wrapper 将 LD_PRELOAD 限定在原厂 btstack，子接收器单独启动并清空 LD_PRELOAD。不要执行 donor 库。
- 目标私有原厂 R9.zip/oem_reference/btstack 本地重新取出，sha256sum 与上述 pin 完全一致。readelf -rW 与 llvm-objdump -d：pthread_cond_timedwait PLT 0x1174f8/GOT 0x2c1504，直接 BL 0x118fb0、0x15e594；pthread_setname_np PLT 0x117834/GOT 0x2c1618，BL 0x236c58、0x236c68；pthread_cond_wait PLT 0x117a8c/GOT 0x2c16e0，BL 0x15ed4c、0x15f1a4、0x2278f0、0x237220、0x2379ac。新增 donor 对照证据，不重复 R97/OSAPI 静态分析；当前不从任意 pthread wait hook 调用 SDK，避免在 OEM mutex 持有时重入。仍需确认实际线程和锁/队列时序。
- 验收分界：现有 RFCOMM 适配、TCP bridge、原厂 callback 解析、QNX ARM build、host 双向测试已具备。原厂生产启动器尚未提供完整真实 owner/配对授权/回收证据，实际注册和 RF 双向传输未执行；没有真实车机环境不能离线宣称完成。固定清单第01步保持 OPEN，不另拆主步骤，也不要求反复上车。后续实际取证应合并到一次明确的集中联调，优先查是否能通过成熟经典 MHI2 方案复用真实接入，而非新增空转包装器。
