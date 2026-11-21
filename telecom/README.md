# AU57x 中国电信 SIM 兼容性 — GitHub 云端 R01

目标车辆：2017 Audi A6L C7 / MHI2_CN_AU57x_K1006_2 / MU1005（QNX ARM）。

**与无线 CarPlay 完全隔离**：此研究保存在 `telecom-mode-trace-r01` 分支；不修改 `main`，也不部署到车机。

## R01 当前证据

- 原厂 `nad` 中 `NadStandardNetworkManager` 有 mode 值比较：`mode=1` 时手动选网构造 `AT+COPS=1,2,%s,0`（GSM）；其他模式 `AT+COPS=1,2,%s`。
- 对自动选网，同一分支对应 `AT+COPS=0,2,,0` 与 `AT+COPS=0,2`；自动情况下模块最终行为仍须验证。
- 实车手动搜到 `CHN-CT 可用`，选网显示无应答；这**不是**LTE 成功注册的证据。
- 原厂 APN XML 共 874 条，缺失电信 460/03、05、11；仅补 `ctnet` 在实车无效。
- 尚未知道电信卡实际 mode 值、mode 的上游赋值路径、模块准确型号/频段、实时 CEREG/REJINFO。

## 云端 R01 内容

- `tools/audit.py`：对既有证据作边界检查、模拟 6 种 `AT+COPS` 构造；如以后提供**不公开的合法原厂文件**，可选执行 ELF SHA256 校验、固件字节与候选访问点离线分析。
- `tests/test_audit.py`：验证 GSM 分支、非 GSM 分支、输入验证、证据状态和移动卡安全边界。
- GitHub Actions 自动运行 Python 测试，生成 `status.json` 和 `RESULTS_CN.md`；产物标记 `EVIDENCE_ONLY`，不假装连上通信模块。
- **没有 OEM 二进制、没有安装包、没有车机写入或基带固件刷写**。GitHub 公共仓库禁止上传 `nad`、`telephone`、车机镜像、SIM 标识和秘钥。

## 下一关键突破

追踪 `telephone → NADServicesProxy → NadStandardNetworkManager` 的 mode 参数传递及赋值条件，确定电信 SIM 的实际路径。在不能拿到真实 ELF 或只读实时日志时，标为 `BLOCKED` 而不是宣称修复。

先保持 MMI 自动选网，不直接对 `/dev/NAD/AT_port` 写命令，不全局替换 `,0` 为 `,7`，不影响中国移动。

## 运行

```bash
python3 -m unittest discover -s telecom/tests -v
python3 telecom/tools/audit.py --out telecom_result
# 仅在本地授权且不上传原厂二进制时：
python3 telecom/tools/audit.py --nad /private/path/nad --telephone /private/path/telephone --out telecom_result
```

**验证范围声明：** GitHub Actions 的绿勾只证明这些无硬件测试通过，**不证明中国电信 LTE 已注册或车机可联网**。

## 2026-11-22 最新云端研发进度（独立分支，无实车写入）

- R03：原厂 nad ARM 取值点（请求+0x10）与 3 个调用点，telephone NADServicesProxy 第三个参数不改动 mode。详见 `docs/R03_OEM_MODE_PATH_CN.md`。
- R04：已实现 `qnx/ct_network_policy_sim.c`，这是**仅对电信 PLMN 的纯字符串假设模拟器**；中国移动/联通和自动选网模板保持原样。已通过 Ubuntu GCC C 主机测试 33 例，QNX ARM 交叉编译结果由 GitHub Actions run 44933653525 记录。该 .so 绝对不可上车安装。
- R05：读取原厂 NADUtil（只读）确认华为 ME909Au-821、Cinterion ALS6A/AH6A 多设备家族分支。尚未证明实际安装哪个型号。切勿在车机运行 NADUtil 冒充只读工具；它会触碰串口。
- R03/R04 研究 CI run 44933415402，64 个 Python 离线/负向测试通过，生成非安装性 ZIP。
- `INSTALL_GATE=BLOCKED`：尚无证据证明电信 SIM 实际 `mode=1`、无修复点/真实 LTE 注册/移动卡回归。
- 原厂 ELF 不上传至公开仓库。继续研发必须解决上游模式分配与当前原车网络注册状态证据，**GitHub 云端模拟通过不能充当运营商联网证明**。

## 工程入口

- `tools/telephone_mode_bridge.py`：真实 SHA 限定的转发函数字节校验（仅在本地有授权 ELF 时可用）。
- `tools/nadutil_families.py`：原厂管理程序模块族的原字节偏移校验（仅在本地）。
- `tools/registration_triage.py`：已有脱敏文本日志的只读分析；不操作设备。
- `tools/release_gate.py`、`tools/transaction_sim.py`、`tools/package_review.py`：安装拒绝、模拟恢复和研究包。
- `ci/build_qnx_policy_sim.sh`：QNX6.5 ARM 源码交叉编译（明确 NO_INSTALL）。