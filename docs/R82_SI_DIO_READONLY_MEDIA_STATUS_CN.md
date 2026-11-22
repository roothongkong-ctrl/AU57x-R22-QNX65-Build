# R82：原厂 SI → 独立 DIO 的只读媒体状态查询（研究源码，不能装车）

固件：MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARMv7。

## 为什么要增加这一步

R81 的跨进程 SI 客户端已经能发送 START / STOP 并收到真实 Unix socket 应答，但 SI 无法仅凭 START ACK 认定原厂 DIO 开始提供 AirPlay 媒体。原来的 DIO 服务端已包含只读 CPRX v3 op6 查询，能返回 12 字节 CPRA，其中第 9 字节为接收器媒体状态 0、4、5 或 7；客户端此前只支持 8 字节的 START/STOP ACK，无法读取并核验这条独立进程状态信息。

## 本轮实际改动

1. `mu_cprx_client_begin_remote(..., MU_CPRX_OP_STATUS, now_ms)` 新增只读操作 6 的客户端支持，保留原有独立 SI 进程和 DIO 进程；不访问 DIO 的内存地址，也不要求另一侧绑定同一 `MuCprxService*`。
2. 客户端对 op6 先读取 **8 字节响应头**；如果结果为 BUSY/失败，不再等待多余字节；只有成功时才继续读取 4 字节媒体标记，合计 **12 字节**。拆分网络读取时等齐各阶段字节；检查 `CPRA`、版本、结果码、状态仅能是 0/4/5/7，后三个保留字节必须为 0。任何不匹配都拒绝，不得将未读完的状态当作成功。
3. 如果 DIO 还未就绪，原服务端会返回错误码 1，客户端将其解释为 `MU_CPRX_CLIENT_BUSY`，保持 `observed_media=-1`，明确不声称媒体已连接。
4. 即使状态读到 7，也仅是**一次 DIO 接收器媒体观测**。原车同手机的内层 iAP2/MFi、视频、音频、旋钮和方向盘仍需各自独立的原厂有效证明；缓存状态码不能自动标记无线 CarPlay 可用。
5. 在 Linux 真实 `fork()` 测试中，子进程先问未启动的 DIO：返回 BUSY；随后从子进程发 START，向父进程 DIO 发 op6 并收到实际 12 字节状态码 4，最后发 STOP；父进程记录接收器仅启动 1 次、恢复 1 次，状态查询没有改变原厂状态。
6. 原 SI owner 在状态查询后若失效，客户端原有授权审查继续撤销确认，不保留永久有效的查询结果。

## 尚未完成

尚无原厂 `smartphone_integrator` 和 `dio_manager` 的真实进程内装载点，未核实车机两个进程实际 UID/GID、目录和 owner 证据；没有端到端 iPhone AirPlay 或 MFi 事件。此项在源代码层面打通 DIO 状态的 **跨进程读取**，但无法证明该状态与手机媒体会话归属一致，不能计作已完成实际无线 CarPlay 功能或装车许可。

保持 `SOURCE_ONLY / NO INSTALL / installable=false / vehicle_verified=false`，禁止公开原厂二进制、账号和密钥。
