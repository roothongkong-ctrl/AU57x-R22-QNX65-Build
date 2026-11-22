# R87：congobg MHI2Q 原生无线 CarPlay → AU57x 首连路径整合（源码测试版）

**基线**：2017 奥迪 A6L C7，`MHI2_CN_AU57x_K1006_2/MU1005`，QNX 6.5 ARMv7。保留所有 R86 原厂 ABI 对照与 R85 SI/DIO 恢复实现。
**安全状态**：`RESEARCH_ONLY`、`vehicle_verified=false`、`installable=false`、`safe_to_flash=false`、`NO_INSTALL`。

## 研究依据与实际复用范围

作者：`congobg/MHI2Q_WiFi_CarPlay`，公开目标 MHI2Q 3638/MU0917；其技术文档记录 5092 兼容路线。作者采用原厂 btstack + 蓝牙 iAP2、原厂 MFi、车辆 Wi-Fi AP、预装入原厂 `dio_manager` 的适配库，以及 Wi-Fi AirPlay 会话，避免外接 CarPlay 盒子。作者二进制程序来自不同固件，**没有**复制入 AU57x 工程。

研究了用户之前上传的 `MHI2Q_WiFi_CarPlay-main.zip`：从内层 `wcp.tar.gz` 只读取 8 个关键参考组件的名称、大小、SHA256 和 ELF 机型，生成 `R87_MHI2Q_REFERENCE_AUDIT.json`，可由 `tools/wcp_donor_reference_audit.py` 重做。公开 donor 使用 PolyForm Noncommercial 1.0.0 许可（详见原仓库），本版只新写了状态机源码，不包含 donor 源代码或编译二进制。

## 实际新增的 C99 工程代码

- `cp_wcp_autoconnect.[ch]`：在原厂已证明配对的蓝牙 ACL 后（BT 角色原厂证据），等待 4 秒，采用经原厂认证的 SDP 验证后请求**原厂 BlueSDK**发起 iAP2 RFCOMM 连接；12 秒连接超时、最多 3 次、失败间隔 5 秒；手机确实没有 iAP2 服务则等待 30 秒；所有回调绑定同一 generation。这个模块并没有凭空连接蓝牙的功能，**`stock_open_iap2` 必须由真实 AU57x 原厂 owner 实现**。
- 状态机在 USB 插入、原厂 owner 丢失、ACL 断开后拒绝继续建立无线链路。取消可能仍在执行的原厂操作时要求真实 `stock_cancel_iap2` ACK；失败则保持 `RESTORE_PENDING`，禁止第二代连接抢占资源。
- `cp_wcp_handoff.[ch]`：将上述真实蓝牙连接证据接到 R44 的 iAP2 CONTROL + 原厂 MFi 认证，再交由 R85 的 `cp_cloud_pipeline` 启动原厂无线流程。**蓝牙 connected 并不等于 CarPlay/MFi connected**：只有 R44 `cp_s4_authenticated()` 证明原厂认证成功，且无线 owner / USB 空闲 / 本代蓝牙连接仍有效时才允许进入现有云端状态机。
- 沿用 R85 原厂 SI 和 DIO **不同进程**、AF_UNIX CPRX 控制、同代际原厂 AirPlay 监听/会话、0x4301 ACK、双频政策、原车媒体真实证明、USB 抢占后双阶段原厂恢复。没有将 MHI2Q 的进程指针跨 AU57x 进程调用。
- `.github/workflows/cloud12-review.yml` 新增该模块 host 负向回归测试；QNX 构建的源文件数由 34 更新至 36，并包含上述两个 C 源文件。

## 已通过的本地测试

- `ci/host_wcp_autoconnect.sh`：首次连接、手机已有 iAP2、重试 3 次、没有 SDP 服务、取消 ACK 失败后的恢复、非法代际、USB 抢占、未配对拒绝、时钟异常。
- `ci/host_cloud_pipeline.sh`：真实 C 函数之间的 R87→R44→R85 移交，明确拒绝蓝牙连接但原厂 MFi 尚未认证的情况；原有 SI/DIO/媒体和恢复回归。
- `python -m unittest discover -s tests -p test_wcp_donor_reference_audit.py -v`：伪造固件、目录穿越、合成清单测试。
- 原有 CI 覆盖和严格 QNX ARMv7 本地 native link 以最新 GitHub Actions 的真实结果为准，**本地 GCC 通过不是 QNX 云端通过**。

## 当前尚缺的原车接口——不准宣称已无线可用

1. 真实 AU57x `btstack` 中原厂 BlueSDK 的手机已配对 ACL / SDP iAP2 / 开启 client RFCOMM / 关闭 client 事件所有者的可调用 ABI，不能照搬 MHI2Q 固定地址。R45 已有**服务端** SDK ABI 证据，不等于 car-initiated client ABI 已核对。
2. 真实 AU57x `smartphone_integrator` 和 `dio_manager` 启动者、同代际 session/USB 活动证明，R86 的 21 个原厂 DIO delegate callback 结构仍未接入真实进程。
3. 原厂 MFi CONTROL、Bonjour/mdns、AirPlay 会话，音视频旋钮等真实数据及媒体见证事件，手机端实际无线 CarPlay 验证。
4. 原车 SD 自动备份/安装/校验/恢复脚本和可靠卸载、断电回滚；本版**无安装脚本、无外置 donor 二进制**。

## 如何核查本版（电脑，不在车上）

```bash
bash ci/host_wcp_autoconnect.sh
bash ci/host_cloud_pipeline.sh
python3 -m unittest discover -s tests -p test_wcp_donor_reference_audit.py -v
# 有用户提供的 donor 原版 ZIP 时，离线复核报告：
python3 tools/wcp_donor_reference_audit.py --donor-zip /path/MHI2Q_WiFi_CarPlay-main.zip --out /tmp/R87_donor.json
```

**下一最短实车开发路径**：把 AU57x `btstack` 已识别的原厂 BlueSDK 管理者接口与 R87 五项原厂回调实现安全、只读观察的生命周期绑定。核实原厂 `RF_OpenClientChannel` 的精确 ABI 与同配对手机会话归属后，才能提车辆只读探针/候选安装版本。先不要插 SD 自动安装本 ZIP；本包只用于电脑研发/编译。
