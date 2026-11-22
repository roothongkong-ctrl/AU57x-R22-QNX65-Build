# R80 — 同一 SI 会话失效 / 有线抢占 / 中断事务自动恢复

适配基线：2017 Audi A6L C7 · MHI2_CN_AU57x_K1006_2 / MU1005 · QNX 6.5 ARMv7。

**发布类型：源代码研究包（SOURCE_ONLY / NO_INSTALL），不是实车安装包。**

## 发现的可复现缺口（R79）

- 当原厂 `si_admitted()`、`owner_exclusive()` 或控制 socket 身份检查突然失效时，`cp_cloud_drive_native()` 可能仅把无线会话标为 stopped，却不自动调用 `native_restore()`；后续 owner tick 也因 stopped 提前返回。需要外部显式调用 `cp_cloud_end()` 才能回收真正的原厂接收器。
- 当有线 CarPlay 插入时，原厂 owner 的周期调用并未自行发现 `wired_idle==0`。如果缺少另一个外部显式抢占调用，无线 START/STOP 仍有可能处于处理中；需要在原厂控制循环**开头**判断有线优先，且原厂恢复 pending 时保留重试能力。
- `mu_cprx_tick()` 在解析、读取消息体、等待 START/STOP 的原厂处理完成期间收到 socket EOF/error，有的分支直接 `drop()`（会清除 `mutating`），可能遗留已经开始但尚未完成的原厂启停操作。

## 实际工程改动

1. `cp_cloud_drive_native()` 在处理任何无线数据之前检查有线占用；自动执行 `cp_cloud_wired_preempt()`。即使上一轮已标记 stopped，仍在后续 owner tick 继续原厂恢复，直至确认。
2. 新增 `native_cleanup_pending` 与 `native_failure()`：原厂所有权失效、客户端失效、AirPlay 状态机 fatal 后，立即撤销无线身份、启动原厂恢复；若原厂停止返回 pending/error，保留状态，在未来原厂 owner tick 重试。不能把停止请求发出等同于原厂已恢复。
3. `cp_cloud_graceful_native_end()` 在正常 STOP 期间也检查有线抢占；有线优先直接进入紧急恢复路径，不等待媒体 BUSY。内层认证、SI owner、蓝牙回调失败改为强制清理路径。
4. `mu_cprx_tick()` 引入 `abandoned()`：处理中的 START/STOP 请求遇到断线、超时、异常附加字节或发送失败，都通过同一个检查回滚，维持 Unix 路径占位直到真实 OEM backend 确认恢复。
5. 正常无故障的媒体暂停/恢复不会强制回滚；旧的非原厂研究模式不新增原厂函数调用。

## 新增本机真实 socket 测试

- 经 Linux AF_UNIX 客户端发送真实 START，随后让原厂 SI owner proof 消失：第一次恢复失败时 socket 仍占位、无线会话已撤销；恢复 owner 后，后续 `drive_native()` 自动重试并完成原厂 stopServer、接口恢复和移除 socket。
- 在媒体活动结束后模拟**有线插入**（原厂 owner guard 临时无法恢复）：`drive_native()` 自动进入 wired priority；owner 恢复后重试到原厂停止确认。
- 独立 backend START 返回 pending，客户端在中途断开：下一次服务端 tick 自动触发 rollback 并保留 socket inode；在 backend restore pending 期间另一个控制器不得抢占地址；成功恢复后才释放 socket。

执行：`ci/host_cloud_pipeline.sh`（ASan/UBSan）、`ci/host_cloud_media.sh`、`ci/host_step04_session.sh`、`ci/host_handoff.sh`、`ci/host_radio_policy.sh`、`ci/host_oem_guard.sh`、`ci/host_csm_fuzz.sh`（50,000 次）、`ci/host_sd_txn.sh`（67/67）、`tests/run_bt_host.sh`，并检查 `tools/cloud12_gate.py`。

## 真实车机接入仍受阻，不得作假

当前未在真实 `smartphone_integrator` 进程中识别/挂接 `si_admitted`、原厂 AirPlay/iAP2 会话、原厂 MFi 认证结果、媒体/方向盘/旋钮的真实回调路径。所需生产端回调只作为严格契约；Linux 上的 OEM callback 均为测试桩。虽然第 07/08/09 步已有可重复代码进展，实际 OEM owner/认证/视频音频/控制必须通过真实环境再验证。原厂固件和用户敏感文件不包含于 ZIP；`installable=false`，尚不能刷车。
