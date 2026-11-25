# 完整成熟上游复用：MU1005 集中研发（NO INSTALL）
上游固定 20c94f51c0f0ae88b154c12e4f604643ba23022c。实车原文 docs/wireless-carplay/WIRELESS_PRODUCTION_CHANNELS.md：MU1316/P5087 主屏画面及输入、AltScreen通过；声音、初次自动重连、持续语义通道、Siri未验收。不能称完整成品。原始 iAP2 守护程序使用 vendored iap2-link/iap2-csm/json，真实 Identify 已修正 5000/5002 配对能力。
本次不是重新实现协议：实际编译完整 Rust daemon（主机 + QNX ARM 并行），主机使用默认 UnavailableAuth；真实执行 donor 程序与当前 MU1005 C ingress/IPC/socket pump 对接，两个 TCP 连接、每连接两代，碎片化 iAP2 marker、回包 SYN、5字节 SDK 片段完成。SDK send 是模拟接收端，验证通信边界而非无线认证。QNX 编译保留上游默认硬件 MFi 实现，不启用实验 signer，不上车。独立 ARM 构建结果不能当作 MU1005 已适配。现有 SD 回滚事务测试与此路线一起执行。
关键差异须直面：上游配置把 channel 限定 1..11，MU1005 已有 live AP 读取及双频策略；必须接实际 AP 参数而非直接改限制。上游 CPRX v3 的 TCP 57343 与本工程 v3 AF_UNIX 证据所有权机制不一致；需要适配真实 SI 控制和接收器，不得恢复不安全 TCP v3。上游默认 /dev/i2c8 MFi 不是 MU1005 已验证设备；优先原厂签名入口与有线排他，不填假 MAC/序列号/配对标识。不把候选塞进 MIB 自动安装目录。
交付门槛：1 真正加载/注册；2 手机发现；3 真实原厂 MFi；4 AP/AirPlay；5 画面/声音/输入/重连；6 一键备份、安装、校验、恢复。此次并行复用完整守护程序和已有事务引擎，尚无真实 OEM 启动或安装。不能把云端编译结果称作可用。
