# R79：原厂 SI 控制客户端实现可确认的 START / STOP 成对协议

适用目标：2017 Audi A6L C7 / MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARMv7。

状态：研究源码、主机测试、云端构建；**NO INSTALL / safe_to_flash=false**。

## 发现的实际缺口

R78 的 CPRX v3 客户端能主动发送 START，核对 AF_UNIX 收到的原厂接收器启动确认；
但正常的无线 CarPlay 结束没有发送 STOP 的程序入口，只能直接调用恢复函数。
此外 `mu_cprx_client_tick()` 在 START 收到确认后一直返回旧结果，未再次核查
会话及原厂接收器是否仍然有效。

## R79 实现

1. `MuCprxClient` 现在保存操作类型（START=1，STOP=2），新增
   `mu_cprx_client_begin_stop()`；复用同一实际 AF_UNIX SOCK_STREAM、
   会话/UID/GID/原厂 SI owner 认证、分段发送及超时处理。
2. STOP 返回 `MU_CPRX_CLIENT_BUSY` 时不停止原厂媒体。原厂接收器收到
   已认证 STOP 后如果状态为 `media==7` 则返回 BUSY；调用者可等流真正退出后再试。
3. STOP 的成功返回必须同时核对**服务端实际回复**与已完成的原厂 backend
   restore，避免发出请求或只收到中间的 `stopServer` 接受便报告恢复完成。
4. 新增 `cp_cloud_graceful_native_end(p, now_ms)`，在原厂 owner 线程中
   驱动非阻塞 STOP、原厂服务端处理、接收 ACK 和最终 `cp_cloud_end`。
   只有正常断开、媒体流实际空闲才执行；有线 CarPlay 插入时仍应调用
   `cp_cloud_wired_preempt()`，不会等待普通的媒体闲置。
5. 进入正常 STOP 后 `cp_cloud_drive_native()` 不会再次发起 START；
   `cp_cloud_is_active()` 也不再把正在退出的媒体会话视作活跃。
   新旧会话不一致时，以前缓存的 START ACK 失效。
6. 原先原厂恢复处理中保留 Unix 套接字占位和 rollback lease 的 R77 安全
   屏障仍在；即使中途有线抢占也会进入原有恢复机制。

## 已执行的本机测试

- 在真实 Linux AF_UNIX 上，先用新客户端执行 START，再测试正在播放时
  STOP 返回 BUSY，原厂 `stopServer` 没有被调用。
- 在媒体状态独立变为空闲后，执行 STOP；原厂恢复经历 pending 和 success
  两个阶段后客户端才确认，原始接口恢复，套接字删除。
- 在完整 `cp_cloud_pipeline` 下重复自动 START -> 等待媒体闲置 ->
  自动 STOP -> OEM 退出恢复 -> 状态机 teardown 流程。
- 验证失效 SI 会话不继续承认之前成功的 START ACK。
- `ci/host_cloud_pipeline.sh`、蓝牙/iAP2/MFi/媒体/双频、5 万组 CSM
  解析样本与 67 项 SD 恢复事务测试全部通过。

## 仍然阻塞

此版本没有真实 QNX 原厂 `smartphone_integrator` 进程内的回调注入点、
原厂认证/会话 owner 证据，也没有任何真实 iPhone / MMI 音视频联通证据。
主机 socket 握手不等于车机 CarPlay 连接；本代码只是可以交付核验的源码。
禁止制作或使用刷车安装包，不能改变固定 12 步中的实车验收状态。
