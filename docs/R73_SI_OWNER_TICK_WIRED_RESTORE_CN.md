# R73：SI 接收器 owner-tick 自动推进媒体阶段 + 有线抢占恢复重试

基线：2017 奥迪 A6L C7 / MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARMv7。
**源码研究，不可安装（SOURCE ONLY / NO INSTALL）。**

## 实际改动

1. `cp_cloud_drive_native` 在原厂 CPRX v3 START 确认、同一代 SI 会话与原厂 AirPlay 单例验证后，调用 `cp_cloud_drive_start()`，并继续由 **同一原厂 owner 线程**推进 `cp_cloud_inner_auth()`、`cp_cloud_media_ready()`。没有增加网络线程，也没有伪造认证或媒体数据。只有原厂提供真正的内层 iAP2/MFi 成功证据，并且 `MuCprxEvidence.media()==7`、四项媒体/HMI 验证均通过时，才返回活跃状态。
2. 保留“等待”语义：内层认证未完成、原厂媒体状态为 0/4/5、部分媒体/HMI 证据缺失、媒体活动后来消失，均返回 `CP_CLOUD_RETRY`；不能将先前的 `MEDIA_ACTIVE` 状态当作永久有效。
3. `cp_cloud_wired_preempt` 在第一轮立即撤销无线控制、标记有线优先，但**每次调用都重试** `native_restore`。只有原厂停机、CPRX lease 释放和原始网络接口实际恢复，才返回 `CP_CLOUD_OK`。之前“第一次返回处理中，但下一次不再执行恢复”的风险被修复。已经接入有线时仍不得将“软件状态机结束”误报为“原厂恢复成功”。
4. `tests/test_cloud_pipeline.c` 新增逐个 owner tick 检查：inner iAP2 未认证 → WAIT；内层认证后原厂媒体 0/4/5 → WAIT；方向盘通道未验证 → WAIT；四通道与媒体 7 到齐 → ACTIVE；媒体降为 0 → WAIT；有线抢占第一次恢复处理中、第二次实际恢复成功。测试通过原有真实本机 CPRX socket 发送 START，但所有 OEM callback 为宿主机桩。

## 本地测试

- `bash ci/host_cloud_pipeline.sh`：通过（ASan/UBSan）
- `bash ci/host_cloud_media.sh`：通过（ASan/UBSan）
- `bash ci/host_step04_session.sh`：通过
- `bash tests/run_bt_host.sh`：通过
- `bash ci/host_sd_txn.sh`：67/67 通过

## 仍未解决

- 没有在真实 `smartphone_integrator` / `dio_manager` 进程内验证运行时地址、线程、原厂回调或 Apple 认证；AirPlay 真实音视频是否进入车机仍未知。
- 真机有线抢占可能已使 `owner_exclusive` 失效，清理接口必须由 OEM owner 在有线抢占之前协调；不能靠这里的主机测试推断车上可以安全抢占。
- 尚无真实 QNX 可安装/恢复工程，项目仍禁止制作可刷写 SD 安装包。
