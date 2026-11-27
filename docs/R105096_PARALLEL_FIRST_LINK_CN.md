# R105.096 首次连接并行复测
R105.095 run 45924898674 已失败；串口仍为 xpt_configure cache initialization failure、/dev/ram0 不存在、mkdtemp errno 89。ARM 编译和 io-pkt 启动成功。QNX 6.5 io-blk 官方说明 ARM 全局缓存可改为 memory=sysram，最小缓存512k。现在用两个独立 runner 验证 sysram + alloc=demand/cache，保留原有实际 AF_UNIX、权限、认证及回滚断言，独立证据包。两个均未通过前不计功能验收；任一成功也只证明 fixture 联合运行，非实车。NAS 继续共享原任务，不重复下载。仅 QEMU 测试环境变更，NO INSTALL。
