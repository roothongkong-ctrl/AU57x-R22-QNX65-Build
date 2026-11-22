# R74：原厂 SI 会话级 iAP2/MFi 与视频/音频控制证明（源码研究，禁止安装）

车机目标：MHI2_CN_AU57x_K1006_2 / MU1005 / QNX6.5 ARMv7。

## 本轮解决的具体问题

R73 从原厂独占的 CPRX v3 接收器读取启动和媒体标志，但
`inner_iap2_mfi_verified(epoch)`、`media_verified(epoch, channel)`
仍然只携带粗粒度 epoch。若旧手机在同一 owner epoch 留下已认证状态，
当前会话可能得到错误的播放放行。

R74 在 `MuCprxEvidence` 增加两项仅由真正原厂 SI owner 提供的接口：

- `si_inner_iap2_mfi(ctx, generation, epoch, session)`：确认当前手机在同一个原厂接收器会话完成内层 iAP2/MFi。
- `si_media_channel(ctx, generation, epoch, session, channel)`：逐一确认视频、音频、MMI 和方向盘四类通道属于当前手机会话。

生产路径 `cp_cloud_bind_rx_service` 缺少任一回调时拒绝绑定。
`cp_cloud_inner_auth`、`cp_cloud_media_ready`、`cp_cloud_is_active`
每轮都核对原厂单例、精确 SI 身份、原厂认证与四通道；任何来源丢失立即
失去 MEDIA_ACTIVE 状态。非原厂演示模式保持已有的研究用证明接口。

## 验收边界

主机 fixture 能按上述合同模拟“旧手机成功，新手机尚未认证”及
“仅音频/方向盘不存在于当前 SI session”的负向测试。
没有读取到真实车机内层 iAP2/MFi 或音视频事件，无法确认原厂 SI
实际提供这些字段。不能把编译或模拟结果作为实车播放证据。
所有源代码仍为 SOURCE_ONLY / NO_INSTALL。
