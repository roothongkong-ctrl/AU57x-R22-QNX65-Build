# R105.285 SI 原厂启动调用者与 DIO 生命周期
复用 R105.284 原厂 DIO spawn 和 receiver；独立并发 2 项：
1. 原厂 SI 精确 SHA256 固件只读定位启动相关 PLT 调用、dio_manager 路径及直接 ARM literal 引用。仅提取新的调用者证据，不宣称已挂接。
2. 新增 SI owner 启动/退出桥，禁止同 epoch 重放，只有真实子进程退出及原厂 owner 确认清理后才允许下一次连接。使用既有缓存编译 QNX ARM，不重建工具链。
保留原厂有线和 MFi。尚缺实际 SI caller 与真实 owner 回调接线；此次产物 NO INSTALL。
