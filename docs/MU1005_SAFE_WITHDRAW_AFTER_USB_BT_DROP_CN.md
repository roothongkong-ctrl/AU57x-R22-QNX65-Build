# MU1005 原厂无线服务退出路径修复（2026-11-23）
以前 au108_live() 在 USB 接管或 MFi/蓝牙失效时把会话置 POISONED，而
au108_withdraw() 只允许 LIVE，且要求 paired_phone / MFi ready / wired_idle。
这造成已发布的 BlueSDK RFCOMM/SDP 服务无法正常撤销。
修复：
- SUSPENDED 独立于 POISONED：禁止无线会话和重复注册，但保留服务句柄待原厂线程清理。
- 撤销只需同一原厂固件运行态、原厂 owner 线程、无待处理回调与真实 unregistration 函数；
  不要求手机仍连接，也不要求有线 CarPlay 不运行。
- 未确认的注册失败保持 POISONED，不自动猜测清理。
- 新增 HOST 回归：USB 已接管仍可安全撤销、线程 owner 暂失不能撤销、等待 callback quiescence。
此修复仍未包含真实 MU1005 蓝牙 owner 注入入口或 iPhone 无线认证；不可装车。
