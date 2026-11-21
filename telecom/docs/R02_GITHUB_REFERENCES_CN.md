# AU57x 中国电信｜GitHub 同类项目筛选及借鉴结果 R02

日期：2026-11-22。适配基线：2017 奥迪 A6L C7，MHI2_CN_AU57x_K1006_2/MU1005。

## 结论

**无可信公开仓库直接证明同版 AU57x 成功接入中国电信。** 不以“能搜到 CHN-CT”推断成功注册。以下只借鉴**状态判别、测试路径和工程流程**，不直接运行其它车型/模块二进制。

| 优先级 | 开源工程 / 原始链接 | 具体可借鉴内容 | 不能直接照搬 |
| --- | --- | --- | --- |
| P1 | [iscle/qemu-mhi2](https://github.com/iscle/qemu-mhi2) | `hw/usb/dev-mhi2-modem.c` 的 +CEREG/+CGREG/+CREG、+COPS、+CGATT、ECM、SIM 仿真；`tools/mhi2/check_modem.py` 的严格协议测试 | 官方 README 仅列 AU37x P5089、VWG11 K3342、POG11 K5126 固件；不支持本车 AU57x K1006_2。模拟 Cinterion ALS6，SIM 及运营商是虚构的，非原车华为 NAD |
| P1 | [ModemManager](https://github.com/linux-mobile-broadband/ModemManager) | `src/mm-modem-helpers.c` +CEREG/CGREG/CREG 多格式解析和 `src/tests/test-modem-helpers.c` 的测试思路；`src/plugins/cinterion/` 包含选网超时、制式差异处理 | Linux/GLib/D-Bus 驱动不能直接注入 QNX 原厂服务；硬件类型和 AT 扩展必须实测 |
| P2 | [tfourj/MIB2-Patcher](https://github.com/tfourj/MIB2-Patcher) | 原厂文件备份、部署前版本识别、安装后恢复原则 | 目前不应据此自动生成 NAD 安装补丁 |
| P2 | [Mr-MIBonk/M.I.B.](https://github.com/Mr-MIBonk/M.I.B.) | 现有 MIB 工具链已在本车使用；后续适合做只读检查、回滚、SD 操作 | 不等于原版已经支持电信 |
| P3 | [sO-Ohan/sim7600e-toolkit](https://github.com/sO-Ohan/sim7600e-toolkit) | 注册状态、网络制式、网卡数据会话独立诊断，不从网络搜索结果得出入网结论 | SIM7600 是不同制式/驱动，AT 私有命令不能照搬 |

## 本次代码借鉴所实现的具体改进

新增独立标准库工具 `telecom/tools/registration_triage.py`，并补充 11 项模拟回归测试：

1. 不把 `AT+COPS=?` 搜网发现（即使运营商标记为“可用”）当作 LTE 注册成功。
2. 仅在能关联查询指令 `AT+CEREG?` 时，将 `+CEREG: <n>,<stat>` 判断为确切状态；缺少请求上下文且多数字字段时保守标记歧义。
3. 分辨 LTE（`CEREG`）与 GSM 电路/分组注册（`CREG`/`CGREG`）及数据附着/激活（`CGATT`/`CGACT`）。
4. 遇到 `REJINFO` 或 `+CME ERROR: 30` 只记录线索，不凭空生成根因。
5. 不保留源日志中的 IMSI、ICCID、IMEI；从不访问 `/dev/NAD/AT_port`，不发送选网命令。
6. 报告始终保留 `OFFLINE_HISTORICAL_LOG_NOT_LIVE`，不会把历史日志误报为实时联网状态。

## 发现的一处重要限制：第三方 MHI2 模拟器的手动选网

查看 `iscle/qemu-mhi2/hw/usb/dev-mhi2-modem.c`：
- 支持 `+COPS?`、`+COPS=?`，以及 `+COPS=0`、`+COPS=3,` 等配置前缀。
- 但严格分支没有为 `AT+COPS=1,2,46003,0`（中国电信手动选网 + GSM AcT）提供完整实现。现阶段不能拿该模拟器运行结果去判断用户实车该命令可用。
- 模拟器证实原厂 MHI2 服务能在其它固件/虚拟设备上完成 LTE 注册和 IP 数据连接，但不证明本车基带和网络可用。

## 继续推进的最短路径

1. 追踪真实 `telephone → NADServicesProxy → NadStandardNetworkManager` 中内部 `mode=1` 的调用/赋值条件。
2. 区分 `AT+COPS` 的 3GPP `<mode>`（自动/手动）与 OEM 程序自己的 `mode` 参数（此前只找到数值比较）；二者不可混淆。
3. 获取实时**只读** CEREG/REJINFO 状态之前，不把 APN、频段、SIM 套餐或代码模式任一项认定为唯一根因。
4. 核实实车模块准确型号：旧日志中的 `ME909AU-821` 只是设备线索；不能套用公开 ME909s-821 的频段参数。
5. 维持中国移动可用、版本校验、R7 备份恢复优先的 NO-INSTALL 政策。

## 工具使用

本地不上传敏感日志的情况下：

```sh
python3 telecom/tools/registration_triage.py --transcript LOCAL_REDACTED_LOG.txt --out triage.json
python3 -m unittest discover -s telecom/tests -v
```

**这份报告不包含已有代码的复制粘贴，不是原车工程移植通过声明。**
