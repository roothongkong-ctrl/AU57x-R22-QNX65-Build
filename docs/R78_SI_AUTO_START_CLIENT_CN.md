# R78：原厂 SI 端首次具备非阻塞 CPRX START 客户端（源码 / NO INSTALL）

适配：2017 Audi A6L C7 / MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARMv7。

## 真正解决的缺口

R77 有独占、可恢复的原厂 SI CPRX v3 Unix 控制**服务端**，但用于启动原厂 AirPlay 接收器的 START 请求一直由主机测试手工组装 32 字节消息发出，项目自身没有对应的 SI 控制**客户端**。仅有服务器无法自动发起原厂接收器启停。

## R78 新增

- 在既有 `cprx_service.c/.h` 中实现 `MuCprxClient`：原厂 SI owner 线程调用 `mu_cprx_client_begin/tick/close`，通过真实 AF_UNIX SOCK_STREAM 非阻塞 `connect`、分段 `send`、`recv` 完成 CPRX v3 START/ACK。保留原厂服务端的 QNX `getpeereid` / SI 会话鉴权。
- 请求以当前 `generation/epoch/session/port` 生成，不接受外部凭空提供的授权标签；客户端核查 socket inode、连接的系统身份，断开或超时直接失败，不伪造 ACK。
- QNX 无 `MSG_NOSIGNAL` 时，客户端沿用 `SO_NOSIGPIPE` 保护，避免对已退出的控制端写入导致 SI 原厂进程收到 SIGPIPE。
- `cp_cloud_enable_native_client` 只有在已经绑定原厂 DIO、已绑定具备原厂身份验证的 SI CPRX v3、无线接管准备妥当时才允许原厂 owner 显式启用。正常 `cp_cloud_drive_native` 每一轮负责客户端发包、服务端推进与客户端确认，不引入第二个控制线程。
- 如果启动之前外层 iAP2/MFi 尚未认证或 Wi‑Fi 未关联，客户端不发包。服务端完成启动和 ACK 后，后续 CarPlay 4301、内层 iAP2/MFi、视频/音频/旋钮/方向盘仍严格依赖真实独立回调；**控制 ACK 不等于手机媒体已接通**。
- 在恢复原厂 AirPlay 前关闭客户端 FD；继续保留 R77 的“原厂 STOP 尚未确认就不得释放 socket 路径”屏障。旧的无客户端方式未删除，仅作为手动研究对照。

## 实际测试

`ci/host_cloud_pipeline.sh` 在 Linux 主机真实 Unix 套接字上用新客户端自动发出 CPRX START，服务端执行原厂接口桩 `startServer` 恰好一次，客户端验证真实 CPRA ACK。负向包含旧 SI 会话、错误 UID、错误 inode、缺少 receiver、禁止重复启动、有线抢占原厂恢复屏障。

还通过 `ci/host_cloud_media.sh`、`ci/host_step04_session.sh`、`ci/host_handoff.sh`、`ci/host_radio_policy.sh`、`ci/host_oem_guard.sh`、`ci/host_csm_fuzz.sh`（50,000 组）、`ci/host_sd_txn.sh`（67/67）、`tests/run_bt_host.sh`。

## 不能虚报的部分

所有 OEM 函数仍使用主机测试桩，尚未在真正 QNX `smartphone_integrator` 进程中运行。原厂进程归属校验、真实 iPhone 内层 MFi 认证、AirPlay 音视频/控制链路及 SD 自动安装尚未完成。QNX 构建仅证实源码 ABI/目标链接，不等于实车通过。此版本 **SOURCE ONLY / NO INSTALL**，不可刷写。
