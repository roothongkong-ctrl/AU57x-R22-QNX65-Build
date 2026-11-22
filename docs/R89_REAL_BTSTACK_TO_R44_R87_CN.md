# R89 — 原厂 MU1005 btstack → 57342/TCP → R44 iAP2 → R87 无线会话

日期：2026-11-22。基线：`MHI2_CN_AU57x_K1006_2/MU1005`、QNX 6.5 ARMv7。

## 这次实际解决的问题

R88 `cp_wcp_r45_ingress` 直接持有 `AuRfIngress *`，但原厂 `btstack` 和我们拟运行的无线控制服务在**不同 QNX 进程**。跨进程直接使用这个指针不成立；即使 R88 主机测试能通过也不能说明真实进程能连起来。

R89 新增 `cp_wcp_r45_remote.[ch]`：直接利用已有 R45 数据帧格式和 R44 原生套接字接收通路，而不是跨进程引用原厂内存。

```text
真正原厂 btstack 蓝牙回调（尚未安全绑定）
  ↓ R45 的原厂 SDK 服务端事件 2/5/4
R45 InboundR5 → AuRfIngress → TCP 127.0.0.1:57342
  ↓ 既有 R44 cp_step04_socket_io → cp_step04_tcp_bridge
R44 iAP2 LINK + 原厂 MFi CONTROL
  ↓ R89 cp_wcp_r45_remote (新接入，真实 socket/epoch/MAC/owner 门禁)
R87 cp_wcp_autoconnect / cp_wcp_handoff
  ↓ R85 独立 SI/DIO AirPlay（仍待原厂运行时绑定）
```

**不伪造认证：** 对方进程仅发送 TCP 帧，并不能证明是真正的原厂 btstack 或真实配对手机。R89 仍强制真实原厂 owner、同一原厂蓝牙 generation、同一 MAC 的配对授权、原厂 SDP 服务发布、真实 fd/session 来源、USB 空闲、会话时代号与 socket 双重认证回调。任何一项取消立即判定无线链路无效。真实的 QNX OEM 来源验证回调尚未提供，所以**仍然不可安装**。

**真实调用链覆盖测试**：`ci/host_wcp_r45_remote.sh` 使用两端真实 TCP 连接和两次拆分写入，走 R44 真实字节解析器到 R87，再验证 MAC、会话时代号、原厂 owner、USB 抢占、原厂 SDP、IPC 状态以及关断后重放全部拒绝。用于测试的 owner 是明确标注的主机 fixture，不是连接车机的证据。

## 新发现：注册参数不能硬填 0

用原厂 btstack SHA256 `51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566` 的真实 ARM 指令进行静态审计：

- `RF_RegisterServerChannel` 候选入口 `0x277a18`，真实 HFP 调用 `0x2464b0` 紧邻先执行 `mov r2,#7`。
- SIM 的第二个真实调用 `0x24dcac` 紧邻先执行 `mov r2,#4`。
- 这说明原厂注册的第三参数依服务而变，**并没有证明无线 iAP2 第三参数该填什么**。R45 原来将第三参数写死 `0`，不得安全用于实车。

改动 `legacy_mu1005/bluetooth/native_binding.[ch]`：原厂第三参数现在必须由 `MuBtOwner.iap2_profile_proven()` 提供**独立验证**。缺少该回调，`mu_bt_native_attach` 直接拒绝；回调未证明则注册包装器拒绝调用原厂函数。不再使用猜测的 `0`，也不拿 HFP=7 或 SIM=4 冒充 iAP2。

只读审计工具：

```bash
python3 tools/mu1005_rf_register_sites.py \
  --btstack /private/MU1005/btstack \
  --out /tmp/MU1005_rf_profile_sites.json
python3 -m unittest discover -s tests -p test_mu1005_rf_register_sites.py -v
```

真实原厂 ELF 从用户既有的私有包中读取，未加入 GitHub 或源码 ZIP。仓库只记录偏移、SHA 和布尔证明边界。

## R89 验收边界

- 已完成：跨进程 R45→R44→R87 的可编译、真实本机 socket 连接适配；原厂静态 ARM 直接调用与注册参数核对；原厂模式未知时 fail-closed。
- 尚未完成：让 R45 代码安全加载到**真实** `btstack` 原厂生命周期 owner/事件线程；从 MU1005 iAP2 服务得到第三参数的真实证据；确认实际 iPhone 的 SDP/RFCOMM/iAP2/MFi 报文。R44 原厂 MFi 对接和原厂 AirPlay 都需要真正硬件/进程事件证明。
- 不包括：OEM 二进制、运行时补丁、MHI2Q 文件混刷、任何声称车机已连接的日志、SD 安装器。

`installable=false`，`safe_to_flash=false`，`vehicle_verified=false`，`NO_INSTALL`。