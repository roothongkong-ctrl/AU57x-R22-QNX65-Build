# MU1005 SI/CPRX 接入边界（NO INSTALL）
基线：2eeefed61133b47f323f5a361030d79eec298048；R85 receiver：7f595231394d97091ec0fee209677b6e45fd50dc。
本轮仅把已通过 host 真 Unix socket 测试的相同源码交叉编译为一个 QNX ARM 共享库，禁止未解析链接符号，并检查导出。链接通过不表示 OEM 进程加载或运行已验证。
原厂接入仍需提供以下真实调用方，不允许以固定 true、测试回调或 UID 替代：
- SI owner：原厂生命周期线程、已构造的 native key/info、实际注册及激活函数、当前 epoch/session，合法的内存读取和清理证明。
- DIO owner：独立原厂同会话媒体观察、iAP2/MFi 结果、四通道状态；R86 原厂两级 delegate ABI 仅证明回调布局，不证明认证结果。
- CPRX：由已验证的 owner 创建的私有 endpoint，独立 SI/DIO peer 证明。不得重复启动接收器。
- 当前没有原厂文件的实时进程上下文证据，不能猜测实例指针、函数入口或把设备发现当作认证成功；保留原车有线及失败恢复。
