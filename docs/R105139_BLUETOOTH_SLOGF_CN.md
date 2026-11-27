# R105.139 Bluetooth 双路定向试验（NO INSTALL）
R105.136 Bluetooth 与 SI/BTStack 一样，libecpp-ne 缺库已越过，实际失败为 slogf 未解析。复用 R105.137 的两种真实日志实现及已有 SDK staging，不重跑 SI/BTStack。Bluetooth 单独两路并行，共享固件提取与工具链；保留原厂字节校验、真实导出/依赖检查及失败结果。R105.138 原厂扫描未找到两个 HID 符号的共享导出，不构造替代函数、不修改 iAP2 原件。仍未通过真实无线连接与原厂服务验收。新增编译缓存输出，后续命中效果待验证。
