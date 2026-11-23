# R103：直接复用 LuKa 原版 iAP2 链路引擎，与 AU57x RFCOMM 桥对接

## 本次真的移植了什么

1. **直接复用上游开源代码**：从既有用户工程包中提取三个 `iap2-link` 文件，逐文件 `git hash-object` 与 LuKa 固定提交 `91e301dfed64bdb1982c036fcf7f0eb568f0bc8a` 比较，均一致。原始源码未改动，保持 GPL-3.0 条款。
2. **新增运行的 Rust 交接代码**：`integration/r103_iap2_bridge/src/lib.rs` 把 MU1005 已有 C 蓝牙桥协议（8 字节 little-endian 头、类型 1/2/3、代号、最长 5035 字节）转换为原版 `iap2_link::LinkEngine` 的 `start/feed/advance_time/feed_eof` 调用，再将 Marker、SYN、LSP、ACK 输出封回原有数据帧。代码能在普通主机进程运行；不接触原厂 ELF。
3. **保留真实事务语义**：拒绝错误会话代号、重复 CONNECTED、非法长度、未完成的数据、终止后的复用；断线或本机套接字丢失立即使这一轮会话终止。没有模拟或绕过 MFi 认证。
4. **两端的实证**：已有 C 模块通过编译且 C 实际生成 CONNECTED/DATA/DISCONNECTED 的头字节。GitHub Rust CI 实际执行上游 iAP2 原始测试、新增帧桥测试，以及 `tcp_lab` 对本机 TCP socket 的双向真实数据流测试，具体以 Actions 结果为准。

## 为什么不直接把 LuKa 的 btstack/graft.c 整份复制到车机？

它包含与 MHI2Q 特定固件绑定的绝对地址、服务初始化点、启动生命周期与厂商 BlueSDK 数据结构。你的原厂是 MHI2_CN_AU57x K1006_2 / MU1005；此前已发现二进制与内存布局不同。直接运行会有高概率损坏蓝牙和有线功能。

## 未完成、不可造假的部分

- iAP2 服务在 MU1005 原厂 btstack 里的真实线程注册入口、SDK 回调与唯一通道号没有实车证据。
- 原厂 MFi 安全认证不在此库中，本版没有“认证成功”测试或绕过手段。
- iPhone 无线连接、Wi-Fi 实际频段、AirPlay 视频、开机自动恢复与 SD 安装候选仍需集成和验证。
- Rust 测试在 Linux GitHub 主机上运行，**不等于 QNX ARM 可执行**。要用于 QNX 6.5 需要可用的目标 Rust 标准库和运行环境，或后续移植到已有 QNX C 工程。现在安装标记为 `NO_INSTALL`。

## 测试命令

```sh
bash ci/host_r103_c_wire.sh
bash ci/host_r103_upstream_link.sh   # 需要 Rust/Cargo（GitHub CI 使用）
bash ci/host_r95_source_compile.sh
```

**不能将此源码/ZIP 复制到 SD 卡刷车。**

## 本机真实 Socket 端到端补充

`integration/r103_iap2_bridge/src/tcp_lab.rs` 提供单连接测试入口：在 `127.0.0.1` 的随机本机端口使用**实际 TCP**接收 C 格式 CONNECTED/Marker DATA/断开报文，调用原版 Rust iAP2 引擎，并把真实 Marker、SYN 重新用 8 字节数据帧送回。测试同时验证错误连接代号被拒绝。本入口不绑定 57342，也不会修改或启动原厂进程，不包含任何真实苹果认证。
