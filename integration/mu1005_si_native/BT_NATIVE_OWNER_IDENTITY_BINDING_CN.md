# 原厂线程身份与 native_binding 实际连接（NO INSTALL）
本轮将此前独立的 OSAPI ActiveObject 身份检查接入 QNX ARM native_binding 的 attach、cert、tick、callback、withdraw、reclaim 实际调用路径。native branch 使用 pthread_self()，不再只依赖外部回调报告线程正确。attach 固定已核验的 libcomm 映射 bias、线程 ID、factory、SharedPtr control 与 AO 对象；以后即使线程 ID 被重复使用，已知对象身份变化也拒绝旧 resident 操作。
从历史原厂 libcomm.so 实际 ELF PT_LOAD 读取 6 个指令字（0x4d878/e2800008、0x4d87c/ebff1d80、0x4e620/e59030f0、0x4e9d0/e280a0c8、0x4ea74/ebff1b78、0x4eab4/e12fff33）；attach 通过 checked read_live 验证映射中的这些字。不宣称这能代替整文件/映射校验。新增测试覆盖每个指令变更、错误 bias、错误 tid、同 tid 对象生命周期变化。
注意：OSAPI 线程身份不等于实际 iAP2 callback 所有权、原厂锁可重入或可安全回收的证明；继续要求独立 owner_ready 和其他原厂证据，不能假定所有 OEM 回调位于该线程。如果原厂回调来自其他线程，需要先确认正确串行化方案，当前将拒绝。外部真实映射、服务对象及 read_live provider 尚未实现，未安装 hook。真实 iPhone MFi/媒体和实车未验收。当前修改是实际源码连接，不是完整可安装版本。
