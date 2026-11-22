# R90：MU1005 原厂 RFCOMM → iAP2 握手 → 原厂 MFi 的真实接入门禁（仅源码，禁止安装）

日期：2026-11-22。固件基线：`MHI2_CN_AU57x_K1006_2 / MU1005`，QNX 6.5 ARMv7。

## 这次修复了什么

R89 的 `cp_wcp_r45_remote_ops().stock_iap2_connected` 只检查原厂 owner、已配对手机 MAC、同一代际、R45 TCP 和 R44 会话存在。收到原厂 RFCOMM 事件 2（CONNECT）后，R44 `link.phase` 已是 `CP_LK_DETECT`，因此 R89 就可能报告 `CP_WCP_LINK_READY`。这是**过早报告 iAP2 已建立**的工程漏洞；RFCOMM 通道连接不等于 iAP2 协商成功。

R90 将服务器先连模式下的 `stock_iap2_connected` 改为每次确认：

1. R45->R44 原有的 live socket、原厂 owner、同一原厂连接 generation、授权 MAC、SDP 发布、USB 空闲和传输来源继续全部有效；
2. R44 实际接收到有效 iAP2 同步标记 `FF 55 02 00 EE 10`；
3. R44 的解析器验证了对端带 **控制会话 10** 的 SYN/LSP；
4. 对端 ACK 验证了本端 SYN 的序列号，R44 将状态推进到 `CP_LK_NORMAL`，同时 `have_peer_syn=1`。

只有 1–4 均成立才向 R87 返回“iAP2 LINK_READY”。随后 R87/R44 仍然需要原厂 AA00→AA01→AA02→AA03→AA05、真实 OEM MFi 和手机身份归属认证，之后才能进入 R85 的 SI/DIO 无线接收流程。**链接层已就绪不等于 CarPlay 已通过认证。**

## 新的真实 TCP 代码回归

- `tests/test_wcp_r45_remote.c`：使用真实 `127.0.0.1:57342` TCP 套接字（本地主机，不是汽车），模拟分段 CONNECT、iAP2 标记、SYN/LSP、ACK；逐段拒绝提早就绪，并验证换代际、取消配对、USB 抢占、SDP 撤销、断线均撤销就绪。
- `tests/test_wcp_r45_remote_mfi.c`：同样经过**真实本机 TCP 字节流**，但用明确标注的模拟证书/签名 API（`MOCK`，绝非真正 MFi）执行 R44 的 AA00→AA05 正常次序，验证只有整套已提交回应与本代手机授权回执完成才返回 `cp_s4_authenticated=1`，撤销授权后即使恢复该布尔标志也不能恢复已吊销会话。
- `ci/host_wcp_r45_remote.sh` 与新增的 `ci/host_wcp_r45_remote_mfi.sh` 会在 GitHub 软件回归执行。**不制造或携带任何 Apple 凭据。**

## 用户以前的原厂文件核对

原厂 `btstack` SHA256 固定为 `51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566`。R89 复核 HFP/SIM 原厂 `RF_RegisterServerChannel` 模式分别为 7/4，**iAP2 的真实 profile 仍未证明**，本版 `native_binding` 继续保持 `iap2_profile_proven` 缺失时直接拒绝注册，没有退回默认 0，也没有照搬 MHI2Q 的函数地址。

## 尚未跨越的验证边界

- 真实 MU1005 `btstack` 进程中的 BlueSDK 回调挂载、原厂事件线程/生命周期 owner 尚未证明，无法安全提供 `iap2_profile_proven` 及 OEM `owner_verified` 回调。
- QNX 编译/QEMU dlopen 即使通过，也**不证明**汽车无线电、实际 iPhone RFCOMM、Apple MFi、Wi-Fi/AirPlay 视频音频；云端测试所有手机/签名/原厂 owner 回调均明确为主机 fixture。
- 本次只修复了“已经有原厂 RF 事件时，等待真正的 iAP2 握手及 MFi 才移交”。仍需真实原厂进程接入，再准备只读车辆验证和 SD 备份/恢复安装器。

`installable=false`, `vehicle_verified=false`, `safe_to_flash=false`, `NO_INSTALL`。
