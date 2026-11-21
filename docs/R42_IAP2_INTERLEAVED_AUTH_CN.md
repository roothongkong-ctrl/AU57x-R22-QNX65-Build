# R42 - iAP2 AUTH 与 Identification 交错消息兼容性修复

从用户存档 R14 的 iap2-csm/control.rs 逻辑回查，`StartIdentification`（1D00）和 `IdentificationAccepted`（1D03）可能与认证 AA00–AA05 交错出现；MFi 的处理器不应把完全独立的控制会话消息当作认证失败。R41 旧处理器遇到非 AA 消息会锁定为 FAILED。

现在 `cp_auth_receive` 对 **完整且长度正确** 的非 AA00–AA05  iAP2 CONTROL帧返回 `CP_AUTH_UNRELATED`，由真正的 OEM 控制会话所有者交给原厂 identification/其他分发处理，**不会**放开认证门禁、设置 MFi 成功或发送任何凭证。AA04、AA05、伪造签名/无原厂 owner/旧会话仍按原有负向门禁拒绝。

三组主机测试覆盖证书前、证书后、挑战签名后交错消息。仍需证明 QNX真实控制会话与原厂 BlueSDK端点; R42 NO_INSTALL。
