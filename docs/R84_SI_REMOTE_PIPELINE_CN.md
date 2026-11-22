# R84：真正把独立进程 SI→DIO 控制链接入无线 CarPlay 状态机

固件基线：2017 奥迪 A6L C7，`MHI2_CN_AU57x_K1006_2 / MU1005`，QNX 6.5 ARMv7。**研发源码，NO INSTALL。**

## 源码上的真实缺口

R81–R83 已有能够在独立进程间发送 START/STOP、查询媒体及同会话证明（op11）的 Unix 套接字客户端。但主流程 `cp_cloud_drive_native()` 仍需 `MuDioNative*`、`MuCprxService*`、`Mu34ApServer*` 同进程对象。根据此前保存的原厂 70 版 ELF 审计，`smartphone_integrator` 与 `dio_manager` 是两个独立 ELF，不能把另一进程的指针传到 SI 里就认为已经可用。这是源码结构性断点。

## 本轮完成的代码

1. 在 `cp_cloud_pipeline` 新增 `cp_cloud_bind_remote_dio()`、`cp_cloud_drive_remote_dio()`、`cp_cloud_remote_live_at()`、`cp_cloud_remote_graceful_end()`，以及独立的 endpoint、客户端和控制状态。原来的同进程研究 API 保留不动；远端模式明确拒绝再绑定原车 DIO/服务对象。这样 SI 端不再需持有另一进程内存指针。
2. 在原 SI 线程驱动 **实际 AF_UNIX START** → 独立原 DIO 线程处理 → 验证 **CPRA ACK** → 使用现有原车 iAP2 bootstrap 产生 4301 及解析真实链路 ACK → 持续发送 **op11** 读取带 `generation/epoch/session` 的同会话媒体证明快照。正常 STOP 经同一个跨进程客户端完成。
3. 改造 `actual_wireless_proof()`：远端模式不要求 SI 持有 DIO AirPlay 对象指针，只以本次 START ACK + 独立 OEM Bonjour、监听状态及当前原车热点关联证明回答 bootstrap。不得仅因监听开启就宣布媒体成功。
4. `cp_cloud_drive_remote_dio()` 只有在同一 SI owner、蓝牙、原厂 4301 对端 ACK、当前连接的原厂 AirPlay session、DIO 内层 iAP2/MFi=1、原厂媒体=7、四路视频/音频/MMI/方向盘都=1、原 SI owner 再次独立确认后，才推进状态机到 `MEDIA_ACTIVE`。**没有这些真实 OEM 回调，车辆不会成功进入该状态。**
5. 远端媒体观测不永久生效：`cp_cloud_remote_live_at(now)` 必须是 500ms 内的最新有效 op11 快照，之后到期自动否认；`cp_cloud_is_active()` 不读取旧快照。原始 owner / 会话一旦不符即否认。
6. 正常退出必须先读到远端实际空闲媒体状态，再经 32 字节 STOP 请求、原 DIO 原厂恢复确认、真实回复，最终才走全局 teardown。若恢复未确认，`cp_cloud_end()` 返回 `RETRY`，不得误报完成。
7. **重要安全阻塞：** 没有经过原厂证实的“远端 DIO 正在播放时有线 CarPlay 强制抢占恢复”原生接口。R84 对远端 DIO 未确认停止的 wired takeover 返回 `RETRY`、撤销媒体快照，**不会假称有线接管或 DIO 恢复成功**。这仍阻断装车，不会静默绕过。

## 测试与证据

- `tests/test_cloud_pipeline.c` 的新增 R84 回归使用 Linux 真实 AF_UNIX socket、分开的 SI/DIO owner tick；原 SI pipeline **没有绑定** `MuDioNative*` 或 DIO 服务指针。主机接口桩一侧实际执行 START，一侧调用原有 MFi/iAP2 链路，使用校验和正确的 iAP2 ACK 报文（仅本机 fixture）驱动 4301，再由 DIO 的 op11 返回内层认证和四路媒体证明；媒体 4 时拒绝，媒体 7 且全证据满足时才对本轮 tick 报告 OK。
- 验证错 epoch、无回调、重复绑定、未收到 START ACK、尚未收到 iAP2 ACK、过期媒体状态（501ms）、方向盘状态丢失、错 SI 会话、未退出媒体时拒绝 STOP、原车恢复未完成时拒绝 teardown。
- R81 单独的 `fork()` 双进程 START/STOP 和 R83 op11 防伪仍通过原有回归。运行 `ci/host_cloud_pipeline.sh`、`ci/host_cloud_media.sh`、`ci/host_step04_session.sh`、`ci/host_handoff.sh`、`ci/host_radio_policy.sh`、`ci/host_oem_guard.sh`、`ci/host_csm_fuzz.sh`（50,000 组）、`ci/host_sd_txn.sh`（67 项）、`tests/run_bt_host.sh` 以及 `tools/cloud12_gate.py` 全部通过。

## 仍不能装车的依据

现有的 `owner.*`、`si_owner_verified`、`dio_server_verified`、`si_inner_iap2_mfi`、`si_media_channel` 尚无从原厂 `smartphone_integrator` / `dio_manager` 的真实进程内运行事件完成的生产绑定。静态原厂符号表能够证明 SI 存在 `servmngt::startApp`，DIO 调用 `iap2_connect` 和 AirPlay 接收器，但**并不证明现有回调在无线会话里已经存在、可稳定调用**。测试使用的是本机 OEM 函数桩，不能代表 iPhone 已经认证或原厂音视频进入 MMI。真实 QNX 的 UID、socket 地址、owner 进程鉴权及前述强制抢占都没有实车验证。

固定 12 步审计仍 `completed_steps=[1,2,3,4,10]`，其余不能标为完成。`safe_to_flash=false` / `vehicle_verified=false` / `SOURCE_ONLY / NO_INSTALL`。
