# R76：SI 控制通道切换为具备内核凭据的 Unix 域套接字（NO INSTALL）

目标：2017 Audi A6L C7 / MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARMv7。

## 实际工程修改

1. 原 CPRX v3 入口 `mu_cprx_open_si`、`mu_cprx_open_backend_si` 仍保留公开符号供 ABI 对照，但现在 **直接拒绝启动**，不再默许 127.0.0.1:57343 TCP 作为有权限的 SI 控制通道。原 v2 研究接口未改变。
2. 新增 `mu_cprx_open_si_unix`、`mu_cprx_open_backend_si_unix`。调用者必须传入已经创建、由当前授权 owner 拥有且权限为 `0700` 的**本机私有目录**；不能使用 `/tmp` 根目录或猜测车机挂载点。函数创建 `cprx.sock`（0600），拒绝预占用或错误权限路径，拒绝覆盖已有套接字。
3. QNX 采用 `getpeereid()`，Linux 宿主测试采用 `SO_PEERCRED`。`MuCprxService` 对实际连接的系统 UID/GID 与真实原厂 SI owner 提供的预期 UID/GID 比较，再调用原有 `si_client_verified` 校验真正 SI 所有权和会话。消息自报的 generation / epoch / session 不是凭据。
4. 在每轮原厂 owner tick 重新核对连接者及 SI 会话，丢失权限时断连；原厂处于不可中断的启停操作时，保留之前的回滚屏障。
5. 清理时只在套接字路径设备号和 inode 都匹配时删除路径；不清理陌生人的同名文件。单会话 SI 服务仍负责 AirPlay 原厂接收器唯一启停与恢复。
6. `cp_cloud_pipeline` 拒绝绑定非 Unix v3 控制服务，避免误将旧 TCP 接口当作原厂授权链路。

## 验证

- Linux 主机 ASan/UBSan 的 `ci/host_cloud_pipeline.sh`：在真实 AF_UNIX 本机 socket 上发出 CPRX v3 START，原厂函数桩只调用一次；拒绝世界可写目录、缺少原厂认证回调、重复 socket、错误 UID、无授权及授权中途丢失；正常控制者可以完成原厂接收器启动和停止恢复。
- 蓝牙 RF、iAP2/MFi、媒体事件、双频策略、50,000 CSM 样本及 67 项 SD 恢复事务测试继续通过。
- QNX 6.5 原生编译结果以对应提交的 GitHub Actions 为准。

## 尚未解决（不可掩盖）

- **getpeereid() 只提供 UID/GID，不提供唯一的 SI 进程身份**。原厂多个进程若共用 root，同 UID/GID 不能区分它们，因此 `si_client_verified` 仍需真正原厂进程/owner 的独立运行时校验。当前仓库只定义接口，不能声称已从原厂 `smartphone_integrator` 读取此信息。
- 必须确认车机上有可用的 Unix 域 socket 提供者、预先创建的可靠私有目录、SI 原厂进程的有效 UID/GID 及运行时生命周期；全部均未实车验证。**没有生产可写目录，也没有制作可装车刷写包。**
- 真正的原厂蓝牙 iAP2/MFi、AirPlay 手机媒体以及 MMI 视频、音频和旋钮方向盘数据仍未端到端运行。

QNX 6.5 原始资料：
- https://www.qnx.com/developers/docs/6.5.0SP1.update/com.qnx.doc.neutrino_lib_ref/u/unix_proto.html
- https://qnx.com/developers/docs/6.6.0_anm11_wf10/com.qnx.doc.neutrino.lib_ref/topic/g/getpeereid.html

项目状态：`REVIEW_ONLY / NO_INSTALL / vehicle_verified=false`。
