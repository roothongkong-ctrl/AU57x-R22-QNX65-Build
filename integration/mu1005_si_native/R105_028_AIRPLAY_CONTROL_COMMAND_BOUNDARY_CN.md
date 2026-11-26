# R105.028 — 原厂 AirPlay 命令分支边界（NO INSTALL）

样本 libairplay.so SHA256 193a4fd9101ec2aa05e7159cfa307b96500810d379ca74a194f172adc13a46b5。离线反汇编导出 AirPlayReceiverSessionControl @ 0x25c80（1660 bytes），解析真实 PIC 常量（GOT 0xaa618，常量基址 0x93340），不重复 R86 delegate 注册审计。

| 比较调用 VA | CFLString 对象 VA | 内联字符串 |
|---|---|---|
| 0x25cc0 | 0x936fc | modesChanged |
| 0x25ce4 | 0x93660 | requestUI |
| 0x25d30 | 0x93714 | updateFeedback |
| 0x25d50 | 0x93738 | hidSetInputMode |

0x25d94 调用 session+0x24 的 delegate，r0=session、r1=command、r2=qualifier、r3=params、栈参数=output/context（context 来自 session+0x14）；该路径随后返回 0，不传播 callback 返回值。0x25e60–0x25e84 为另一条 platform control 调用及返回值传播路径。必须区分两个路径，不能把 delegate 返回值作为无线 raw 数据确认。

原厂 libairplay.so 与 dio_manager 未找到字面 ASCII iAPSendMessage；这不证明所有可能编码或间接路由都不存在，但不能假定 stock library 自带上游无线消息实现。原厂 dio_manager 仅导入 iap2_connect/disconnect/eap_* 与 GPS 发送等，未导入 iap2_raw_msg_send。通用 AirPlayReceiverSessionSendCommand 导出仍在，不等于无线双向绑定。

结论：R105.027 的 updateMode 与上述 modesChanged 等 HMI control 通道不可直接作为 raw iAP2 接收端。保留现有 upstream inner_airplay adapter，下一步核查真实 HTTP /control 命令解析到 delegate 的链路、回调线程和响应契约，再决定最小适配。此报告仅静态证据，无新运行时代码、无实车或 MFi 验证；不重跑旧 CI，不提交原厂 ELF。