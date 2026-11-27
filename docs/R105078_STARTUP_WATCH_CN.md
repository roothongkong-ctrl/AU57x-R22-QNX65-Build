# R105.078 启动事件与真实状态之间的工作线程桥
新增有界 worker tick，在已提供的生命周期 lease 内读原厂启动及停止指令和实际 service state。trace 比 started flag 早时允许有限重试；无 lease 不读，错误映像不通知，重复 trace 不延长窗口。ready 回调在 lease 内执行，仅为候选通知，不注册服务。真实原厂 lease、实际 interposer、SDK 接入仍未实现。新增 host fixture，ARM 原生库编译；未完成云端测试。固件新发现的国内分享仍为网盘，尚无可用 HTTPS 直链，不自动绕过限制。
