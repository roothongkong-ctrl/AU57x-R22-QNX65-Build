# R88：实测用户原厂 MU1005 蓝牙二进制，落地原厂 RFCOMM → R87 接口

**固件基线**：2017 奥迪 A6L C7 PA，`MHI2_CN_AU57x_K1006_2/MU1005`，QNX 6.5 ARM32。

**静态审计日期**：2026-11-22。真实用户资料存于本地私有分析目录，**不上传原厂 ELF 到公共 GitHub，也不包含在 R88 ZIP**。没有连接 iPhone 或汽车，禁止安装。

## 本轮证据（不是从其他 MHI2Q 直接照搬）

执行 `tools/mu1005_bt_iap_path_audit.py`，对 `btstack` 和 `bluetooth` 两个原厂 ELF 逐字节核对 SHA256 和 QNX/ARM32 ELF 结构：

| 文件 | 精确 SHA256 |
|---|---|
| btstack | `51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566` |
| bluetooth | `7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272` |

- 使用原 R45 的 `native_binding_words.h` 校验原厂 `btstack` **40/40 条 ARM 指令**。
- 原厂 `bluetooth` 的字节中找到 `bluetooth.enableIap`、`SERVICETYPE_IAP2`、`iapEnabled`，说明蓝牙控制层已有 iAP 相关配置、服务类型。
- 原厂 `btstack` 含 `BtJobQuerySdp`、`BtJobStartService` vtable、`CON_BTSTACK_IA_RFCOMM`，还有 `OBEX: Client RFCOMM Event`。因此 **原厂 SDK 具备 RFCOMM server、SDP 和某些 client 连接实现**，但这不等于知道 iAP2 的 client-open 函数签名。
- 与 [joeyQuery 的原始 MHI2 MU0678 研究](https://github.com/joeyQuery/MHI2-wirelessCarPlay/blob/main/traces/TRACE-016-iapdevice-pathname-dataflow.md) 对照：在这份 MU1005 `btstack` 中，没有原始字节 `IapDevice` / `IapServices` / `/dev/iapDevice` / `CIapBTChannel`，**不能据此就说该通道一定不存在**（可能由运行时构造、动态服务提供）。目前没有证据允许直接使用 MU0678 的地址/设备路径。

完整只包含偏移和布尔字段的审计记录 `docs/R88_MU1005_BT_IAP_PATH_AUDIT.json`。

## 真正新增的可编译接线

`project/source/src/cp_wcp_r45_ingress.[ch]` 直接接上现有 R45/R39 原厂 BlueSDK RFCOMM 服务端 event 2 → `AuRfIngress` → R87 `cp_wcp_autoconnect`。

此路径是 **phone-initiated inbound/server-first**，不冒充车机主动 client 连接：

1. 原厂 `btstack` 经过先前定位的 RFCOMM 注册、SDP 发布和配对身份确认，产生一次真正的 RFCOMM 已接通事件；R45 把它写进 `AuRfIngress`。
2. `cp_wcp_r45_bind()` 必须同时取得原厂 owner 的独立回调证明：当前 BT generation、现有 SDP server 发布、同一配对手机 MAC、R45 本代本次 RF generation、桥接传输有效以及 USB 空闲；任何条件不符立即拒绝。
3. `cp_wcp_r45_ops()` 提供 R87 已有 `cp_wcp_bt_ops` 接口。当同一原厂 RF 会话仍连接且所有证据依然成立，R87 的 `stock_iap2_connected()` 才返回真实传输连接状态。
4. 原有 R87 `cp_wcp_begin_cloud()` 继续要求 R44 原厂 MFi 认证，只有真正 CONTROL ACK 和 session-owner 证据才能继续 SI/DIO/AirPlay；BT 连接本身不得放行 CarPlay。
5. 断线 / USB 插入 / RF generation 更换 / BT owner 失效，所有同代证明立即失效；无第二次复用。本 server-first 适配器禁止调用未核对的 BlueSDK 原生 client SDP 和 client-open。

新 C 模块在 Linux host 用真实已有 `AuRfIngress` 和 R87 状态机编译运行，包含错误 MAC、RF generation 重放、USB 抢占、失去 owner、丢失桥接及 server/SDP 不存在的拒绝测试。与原厂实车电话连接**不能混淆**。

## 第 2 步还差什么

- **高优先级**：原厂 `btstack` 的安全加载/attach 与原厂线程/锁/回调 owner 的真绑定。R45 40/40 静态指令匹配不代表在真实进程可直接调用。随后需真实 iPhone iAP2/MFi 响应。
- **第二候选**：对照 `BtJobQuerySdp` / `OBEX` 的 client RF 调用路径，逆出 MU1005 `RF_OpenClientChannel` 精确 ABI，再考虑 R87 car-initiated 路线。没有之前不要填固定地址。
- 第 3/4 步（AirPlay + SD）尚未被本轮静态审计证明完成。

## 复现方式

```bash
bash ci/host_wcp_r45_ingress.sh
python3 -m unittest discover -s tests -p test_mu1005_bt_iap_path_audit.py -v
# 私有离线原厂文件，绝不能加入公开仓库：
python3 tools/mu1005_bt_iap_path_audit.py \
 --btstack /private/mu1005/btstack --bluetooth /private/mu1005/bluetooth \
 --r45-words project/source/legacy_mu1005/bluetooth/native_binding_words.h \
 --out /tmp/r88_oem_audit.json
```

**严格边界**：`original_file_unchanged=true`, `vehicle_verified=false`, `installable=false`, `safe_to_flash=false`, `NO_INSTALL`。
