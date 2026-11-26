# R105.004｜MU1005 原厂 iAP2/蓝牙一次性只读探测

日期：2026-11-26
状态：PASSIVE_DIAGNOSTIC_ONLY / NO_INSTALL / NO_CONFIG_CHANGE

## 目标

R93 旧脚本只查 btstack 进程、connectivity.json 候选路径和 preload 日志，无法回答 R105.003 的核心问题：车机上实际运行了哪些蓝牙/iAP2 进程、加载了哪个 iAP2 driver、实际配置文件位于哪里。

R105.004 在 R93 基础上扩展为一次性只读盘点，目的是让下一轮离线反汇编可以使用实车真实配置路径与运行时模块映射，不再凭空猜路径。

## 脚本

`tools/R105_004_oem_iap2_readonly_probe.sh`

执行位置：M.I.B. → Advanced Settings → Run Custom Script。
结果目录：SD 卡根目录 `R105_004_IAP2_PROOF/`。

## 采集项目

1. QNX 版本与当前进程列表；
2. `bluetooth`、`btstack`、`connectivity_launcher`、`dio_manager`、`mm-ipod` 的进程信息和已映射模块；
3. 原厂可执行文件及 `ipod-drvr-iap2.so` 候选路径、文件大小与可用时的 SHA256；
4. 在 `/eso`、`/etc/mm`、`/mnt/app/eso` 范围内按文件名查找 `connectivity.json`、`bluetooth*.json`、`iap2.cfg` 等配置；
5. 对找到的配置文件制作 SD 本地只读分析副本，并记录哈希；
6. 记录已有 preload 日志是否存在。

## 安全边界

- 不写车机分区、不修改 JSON/INI、不改权限、不重启进程；
- 不加载 LD_PRELOAD、不调用 BlueSDK、不触发 RFCOMM 注册；
- 不更改有线 CarPlay，不更改蓝牙配对；
- 只在 SD 卡创建输出目录和诊断文件；
- 配置副本可能包含蓝牙 MAC 地址等设备信息，`umask 077` 限制本地文件权限。不要公开上传到 GitHub；只通过当前私密会话提供给分析使用。

## 输出

- `diag.txt`：主要进程、模块映射、已知文件和运行环境；
- `candidate_paths.txt`：按文件名发现的配置实际路径；
- `configs/`：发现的配置文件副本，供离线检查；
- `find_errors.txt`：目录搜索的错误/权限提示；
- `README.txt`：结果与隐私提示。

## 本轮状态与限制

脚本已提交源码，但**尚未在真实车机执行**。它只能采集运行时线索，不能证明 iAP2 RFCOMM 已连通，也不代表无线 CarPlay 已实现。

## 采集后研发动作

- 用真实配置路径对照 R99 的 JSON/INI parser 和配置键；
- 用真实进程模块映射确认 `ipod-drvr-iap2.so` 是否被加载，以及加载到哪个进程；
- 把实际 `iap2.cfg` 与原始样本逐字节比较，确认 Bluetooth section 是否被解析；
- 若驱动没有加载，先追启动器依赖；若驱动已加载但无 Bluetooth component，再回到 R104 的 factory/identifier 反汇编；
- 对 `bluetooth` 和 `btstack` 真实模块映射确认后，继续定位服务注册与回调 owner。

**结论：R105.004 是只读证据采集器，不是安装包。** `installable=false`, `safe_to_flash=false`, `vehicle_verified=false`.
