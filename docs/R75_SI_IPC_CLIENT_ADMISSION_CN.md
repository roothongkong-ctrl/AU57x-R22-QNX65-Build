# R75 — SI 控制端口来源身份验证（源码研究 / NO INSTALL）

车型：2017 A6L C7，MHI2_CN_AU57x_K1006_2 / MU1005，QNX 6.5 ARMv7。

## 修复的问题

现有原厂 CPRX v3 本机 TCP 管理端口 57343 只检查 generation、epoch、
SI session 和 owner admission。这些是消息字段和当前状态，不能证明连接者
就是原厂 smartphone_integrator。本机其他进程理论上也可能传入这些
字段来触发同一 AirPlay 单例的 START、STOP、媒体元数据调用。

## 修改

- `MuCprxEvidence.si_client_verified(ctx,accepted_fd,generation,epoch,session)`：
  原厂 SI 运行时必须基于内核提供的本地连接进程身份及真正 SI owner 校验。
  不得把请求里的 session 字段或 TCP 127.0.0.1 当成凭证。
- CPRX v3 `open` 缺少该验证接口则拒绝开放服务。
- `mu_cprx_tick` 在 `accept` 之后、读取请求前先做验证；每次处理已连接端口时再次检查。失败立即拒绝连接，已开始的原厂操作继续执行原来的安全恢复语义。
- CarPlay 原厂 SI 会话绑定要求同一 `si_client_verified` 接口，不允许旧模式绕过。
- 宿主机测试发送真实 TCP 连接：未授权连接被拒绝，不会调用 `startServer`；允许连接后仍需正确的 iAP2/会话、监听等证明。

## 特别限制

**这里没有证明 QNX 原厂 SI 的具体进程验证接口已经拿到。**
Linux 主机测试的 `getpeername` 和授权标志不等于 QNX 进程凭据，不能作为
装车代码使用。原厂 SI 调用者真实身份仍为第 07 步未解决的运行时边界。
只交付公开源码和测试，禁止改写车机或声称无线 CarPlay 已成功。
