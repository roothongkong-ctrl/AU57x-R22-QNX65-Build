# R45：AU57x / MU1005 原厂 BlueSDK RFCOMM 注册接线（研究版，禁止安装）

## 这次真正复用的原始资料

从用户以前提供的 `R14_FINAL.zip`，直接恢复 `legacy/source/bluetooth/` 的
`native_binding.[ch]`、`native_binding_words.h`、`native_event.[ch]`、
`inbound.[ch]`、`inbound_r5.[ch]` 共 **4个原生C模块和5个头文件**。
这些是用户既有工程，而不是从 MHI2Q 固件强行复制地址。当前主线R44负责 iAP2/MFi 链路，
这4个模块则实现原厂 BlueSDK 的真实 RFCOMM 注册调用面和SDP发布生命周期。

**精确固件匹配：** 用户保存的原厂 `btstack` SHA-256
`51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566`。
使用旧R14 `native_binding_words.h` 中的 40 条（VA+ARM指令）在本地从ELF可执行PT_LOAD查验：
**40 / 40 地址位于RX段，且32位指令完全匹配**。审计结果保存于 `docs/R45_MU1005_BTSTACK_STATIC_RECHECK.json`。
原厂 `btstack` 文件未上传GitHub。

## 原厂接口，从R14直接恢复

`mu_bt_native_attach()` 在 QNX ARM32 编译路径中只有满足外部提供的
`owner_ready`（原厂进程/线程/已初始化服务、全镜像哈希和独占锁）、
`transport_ready`（本地iAP2 daemon传输可用）、
`read_live` 和 `paired_authorized`（实际配对身份）才尝试注册。

- `0x277a18`：RFCOMM注册候选函数（输入持久176字节通道对象，回传通道号）；
- `0x2779b4`：注销；
- `0x207244` / `0x2071c4`：SDP记录的发布与撤销；
- `0x27733c`：接受或拒绝连接；
- `0x277074`：最大载荷；
- `0x2771f8`：异步发送，保持100字节描述符直到回调事件6；
- `0x2773f8`：断开；`0x27715c`：窗口/credit；
- `0x276f74`：从原厂通道查询连接关联对象，用于MAC与配对授权。

这是R14历史源码中**推定的调用签名**与经二进制验证的函数入口。
40条指令匹配并不证明运行时ABI、上下文锁、当前原车闪存哈希、SDP实际发布或手机RFCOMM成功。

## 蓝牙 → iAP2 的正确方向

在无线CarPlay引导阶段：手机RFCOMM → BlueSDK事件回调 →
原厂R14 `InboundR5` 的非阻塞socket桥 (127.0.0.1:57342) →
目前R44 `cp_step04_link` iAP2引导链路 → 原厂MFi适配。

**不必**把引导RFCOMM当作 `dio_manager` 的 `/dev/ipod0` USB资源设备。
原厂DIO的第二段iAP2-over-AirPlay仅在Wi-Fi会话建立后启动，属于后续环节。
所有通信需要同一次连接的真实owner证据，并不能因此认定已完成无线CarPlay。

## 云端验收边界

- 宿主机运行R14带的 SDP注册/回滚、BlueSDK事件解码、RPC队列测试。
- QNX原生编译增加四个模块，QEMU只验证可加载符号，**绝不执行硬编码原厂入口**。
- 主线R44的 MFi 成功仍必须由手机真实回执及原厂owner证明，严禁模拟授予。
- 完整授权的原厂进程加载点、真实SDK线程/锁、绑定生命周期目前仍未证明：**STEP04 PARTIAL / NO_INSTALL**。
