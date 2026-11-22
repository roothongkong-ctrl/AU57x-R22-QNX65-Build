# R72：SI 单独拥有原厂 AirPlay 接收器启停权（NO INSTALL）

**对象：** 2017 Audi A6L C7、MHI2_CN_AU57x_K1006_2 / MU1005。

## 解决的具体冲突

R68/R69 曾允许 `cp_cloud_prepare_receiver()` 在 SI 接收器服务使用的
`MuDioNative` 对象上执行 `startServer`。同时 `MuCprxService` 的
CPRX v3 START 会调用相同 `mu_dio_start()`，理论上可造成两套独立
启动路径竞争同一个原厂 AirPlay 单例。

R72 现在：

1. 在 CPRX v3 OPEN 后、START 前，通过 `cp_cloud_bind_rx_service()`
   绑定原厂 SI 所有的单独控制服务。严格验证 generation、epoch、
   session、端口 5000、native 对象身份及 SI admission。
2. `cp_cloud_drive_native()` 只在原厂同一线程调用 `mu_cprx_tick()`，
   消费**真实控制服务套接字**收到的 v3 START；不再独立调用
   `mu_dio_start()`。
3. 控制服务必须成功启动并发送完整确认后，`cp_cloud_native_rx_live()`
   才可成立；此时 CarPlay 流程只轮询已启动接收器，不会重复 START。
4. 原厂 SI 会话错误、旧 generation/epoch、无线/有线所有者失效时均
   拒绝进入就绪状态。结束和恢复仍须经过原厂 stop/restore barrier。

## 验证

`bash ci/host_cloud_pipeline.sh` 真实建立 Linux 127.0.0.1 CPRX v3 TCP
连接，传递 32 字节请求并收到 8 字节确认，验证原厂函数桩仅 START
一次；另测错 SI session 和 epoch、未 START 时拒绝、媒体状态、恢复重试。
`bash ci/host_cloud_media.sh`、`bash ci/host_step04_session.sh`、
`bash ci/host_sd_txn.sh`（67 测试）、`bash tests/run_bt_host.sh` 与
`python3 tools/cloud12_gate.py` 均通过。

**限制：** 测试套接字是真的，但原厂对象、原厂认证、iPhone、手机媒体
状态均由主机 fixture 模拟。未取得真实 SI 进程中蓝牙/iAP2/MFi 或
视频音频输出证据，不构成可刷写车机固件，也不等同“无线 CarPlay 可用”。
