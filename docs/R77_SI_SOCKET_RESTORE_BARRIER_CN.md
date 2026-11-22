# R77：原厂 AirPlay 恢复完成之前保留 SI 套接字占位（源码研究）

**适配基线：** 2017 Audi A6L C7 / MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARMv7。

## 真实缺陷

R76 `mu_cprx_shutdown()` 在调用原厂 backend.restore() **之前** 就 unlink
`cprx.sock`。当原厂 AirPlay 返回“尚在停止/恢复中”时，该路径已被释放，
第二个进程可能重新建立相同监听端点，进而与未清理完成的原厂单例争夺控制权。
只检查会话 ID、UID/GID 不能消除这样的生命周期竞争。

另一个缺陷：`open_service()` 未检查传入的 `MuCprxService` 已经持有
控制监听器和原厂恢复租约。如果第二次 OPEN 使用了**另一个目录**，
旧路径没有名称冲突，`memset(s,0)` 将覆盖原有服务状态，导致资源泄漏
及真实会话恢复链断裂。

## R77 修复

- 调整 `mu_cprx_shutdown()` 时序：先拒绝新的连接并关闭监听 FD，
  **保留 Unix socket 路径**。仅当原厂 `restore()` 正式返回成功后，
  使用既有 `st_dev + st_ino` 对照解除该路径占位。
- 恢复返回 pending 或 error 时，保持单个 `MuCprxService` 持有
  rollback 租约；再次执行 shutdown 时继续重试，不会允许其他
  SI v3 controller 在未完成恢复前拿到同一 socket 路径。
- `open_service()` 在任何文件/套接字副作用之前检查同一服务对象
  是否还持有未释放的 backend lease，拒绝不同目录的重入 OPEN。
- 不更改原有 BlueSDK/iAP2/MFi、AirPlay 接收或媒体通道；不假设
  原厂已有新的回调或 QNX 设备路径。

## 可重复测试

`bash ci/host_cloud_pipeline.sh` 使用真实 Linux AF_UNIX 连接和 OEM
函数桩验证：

1. 已占用接收器重复 OPEN（不同目录）被拒绝，原监听器仍可用。
2. 原厂 STOP 第一次 pending 时，监听 FD 已关闭但 inode 仍保留；
   其他 controller 不能在该路径重新 OPEN。
3. 原厂 STOP 第二次确认恢复时，只有原 socket inode 被删除；
   原始网络接口恢复，私有目录可删除。

并执行原有蓝牙、iAP2、媒体、双频及 67 项备份回滚回归。

## 未完成 / NO INSTALL

需要真正原厂 `smartphone_integrator` 接口验证、真实 QNX 进程权限、
真实内层 MFi 及 iPhone 无线音视频联通；UID/GID 不等于唯一进程身份。
本次只交付研究源码，没有可安全装车的补丁。
