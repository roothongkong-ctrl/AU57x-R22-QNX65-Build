# R71 — 原厂 SI 会话/媒体状态对接（源码研究，NO INSTALL）

目标固件：MHI2_CN_AU57x_K1006_2 / MU1005；不是 MHI2Q 的复用二进制。

## 实际新增

- cp_cloud_bind_rx_service 将同一原厂 MuDioNative 绑定器与 MuCprxService v3（SI epoch / session ID）关联。核对来源、generation、原厂 owner、真实启动确认及 SI admission；未启动或错代际不可绑定。
- cp_cloud_native_rx_live 动态检查接收器是否仍在同一会话；只有 MuCprxEvidence.media 从原厂获得状态 7 时才允许项目的“媒体活跃”判断。值 0/4/5、旧 session、owner 失效均不能通过。
- 原厂 DIO 模式拒绝通用原始字节转发。主视频音频由原厂 AirPlay 解码与渲染，避免把测试转发器当作生产解码器。
- 退出或有线抢占前，先关闭 SI 的 receiver lease，等待 stop 和恢复网络接口的确认。原厂返回 pending 时保留对象和重试能力。
- 主机测试覆盖本机 CPRX socket、会话身份、0/4/5/7 媒体标志、原厂接口恢复分两次完成以及阻止通用字节注入。

## 未被证明

原厂 SI 在真车里创建/授权 DIO manager、真实 iPhone 内层 iAP2/MFi、真实视频/声音、方向盘控制、动态原厂事件回调来源均未实车验证。测试的电话和媒体证据均为桩函数。没有可装车安装包，工程仍 NO INSTALL。

架构参考：luka-dev/mib2q-carplay-rgi 和 congobg/MHI2Q_WiFi_CarPlay 的公开职责分离设计；它们的固件平台不同，不能移植二进制地址。运行复验：bash ci/host_cloud_pipeline.sh && bash ci/host_cloud_media.sh。
