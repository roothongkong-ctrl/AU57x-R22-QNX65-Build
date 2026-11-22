# R85：有线 CarPlay 抢占时 SI/DIO 双重原厂授权的紧急恢复（NO INSTALL）

基线：2017 Audi A6L C7 / `MHI2_CN_AU57x_K1006_2 / MU1005` / QNX 6.5 ARMv7。

## R84 的真实未解决问题

R84 原厂 SI 与 DIO 分开进程控制已支持 START / 状态查询 / 普通 STOP，但 DIO 原车媒体 `media==7`（仍播放中）时普通 STOP 必须返回 BUSY。有线 CarPlay 插入后，远端模式此前只能永久返回 RETRY，无法向 DIO 请求原厂强制恢复。直接删除套接字或假设系统自动恢复会造成 AirPlay singleton/网卡控制权冲突，不能装车。

## R85 的源码实现

1. 在 CPRX v3 新增专用操作 `MU_CPRX_OP_WIRED_RESTORE=12`。与普通 `STOP=2` 不同：允许原车 media=7 时启动原始 DIO 后端 `restore`，但仅限有**两个彼此独立**的同 SI generation/epoch/session 的有线接管证明：SI 的 `MuCprxRemoteEndpoint.si_wired_takeover_verified`、DIO 的 `MuCprxEvidence.dio_wired_takeover_verified`。任何回调缺失、错误状态或不同会话都拒绝。
2. 客户端每次 `begin/tick` 都检查 SI 原厂有线证明以及原有 SI 进程/会话认证、系统内核对端 UID/GID、DIO 原始进程身份回调。服务端在每次 `handle` 时重新检查 DIO 原厂物理有线证明，绝不相信线上的“有线=true”或全局 `media` 标志。
3. 服务端只有执行**真实原始** `backend.restore()` 且返回“已完成”后才生成 ACK。返回 pending 时保持 socket 地址占位；DIO 授权在恢复中途失效则转入原有的 fail-closed 清理，仍须完整恢复，不能把清理开始误认为已恢复。
4. `cp_cloud_wired_preempt()` 第一次取消普通无线发送并标记待恢复；后续在独立的 SI owner monotonic tick 中执行 op12。重复调用不会误杀正在进行的恢复事务。远端必须收到 DIO ACK 才能进入 `remote_phase=4`。
5. **把 DIO 恢复确认与 SI 会话 teardown 分开**：DIO 已恢复但 SI `teardown_confirmed` 尚未就绪时，周期 tick 保持 RETRY；SI 真正完成最后收尾后才报 OK 并保留 `remote_teardown_confirmed`，不重复启动/停止原厂接收器。
6. 正常无线用户主动结束仍走 R79 的 media-aware STOP=2，普通无线受控媒体播放中不能被 op12 意外绕过。缺任意原厂证明，则 R85 仍 fail closed 并保持“不知道何时恢复”的阻塞状态。

## 已有的离线验证

在 Linux 本机真实 AF_UNIX 控制链接 + OEM 行为接口桩中，测试媒体始终为 `7` 时：SI 自称已插线但 SI 回调缺失，拒绝；SI 确认有线但 DIO 尚未确认，拒绝且不停止；DIO 独立确认后，原始 DIO restore 经 pending 阶段再返回已恢复，socket 地址期间不能被第二实例占用；DIO ACK 之后还要求独立 SI 最终 teardown 才报 OK。原有蓝牙、iAP2、AirPlay/双频、5 万组 CSM fuzz、67 项 SD 回滚恢复主机测试继续通过。

## 实车研发阻塞（非常重要）

新增 `si_wired_takeover_verified` 与 `dio_wired_takeover_verified` 目前是**必须由原厂真实运行时提供的接口契约**，没有在你的原厂 `smartphone_integrator` 或 `dio_manager` 中定位可安全调用的生产回调，主机测试里的 `r85_` 函数是测试桩而不是原厂接线。即使 QNX 交叉编译通过，仍不可声称有线接管已在车上成功。原厂真正会话认证、音视频、旋钮方向盘仍未实车验证。NO INSTALL / `vehicle_verified=false` / `safe_to_flash=false`，没有刷写文件。
