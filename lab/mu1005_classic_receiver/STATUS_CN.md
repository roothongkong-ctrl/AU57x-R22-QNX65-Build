# R105.308 经典 MHI2 donor 原始 receiver 运行观察
R105.307 run 46135776135 已成功。MU1005 btstack、connectionmanager 与 donor 三组基线无 SHA 匹配，不能只替换校验值。两个 receiver 均 DT_NEEDED libc.so.3、libsocket.so.3、libm.so.2；之前仅 app70/stage2 扫描未找到基础库，不等同于原车不存在。先在已有真实 QNX6.5 SDK/QEMU 隔离环境并行运行 790/791 原版 receiver，检查加载及早期退出原因；不接网络、车机、手机，不改原厂或 donor，不伪造 MFi 或许可。使用原厂目标哈希及后续实际 ABI 对照作为适配基准。QEMU PID 或退出仅是加载/早期运行证据，不代表 MU1005 连接、认证、HMI 或可安装。保留现有原厂及 Congo 路径。NO INSTALL。

## R105.309
R105.308 两个任务均在 U-Boot 自动引导阶段超时（未到 QNX START），接收器没有执行。恢复默认网络设备拓扑并限制对外网络；连续发送回车中止 autoboot，再执行既有 virtio 启动命令。只修复启动环境，不修改两个 donor 文件、不重建 SDK。未确认接收器兼容。