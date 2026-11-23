# R99｜切换到原厂 iAP2 路线，完成两份配置的离线适配工具

**这轮只承诺并完成一项：原厂 iAP2 配置双开关的自动识别、离线生成、原文件回滚和独立复核。** 不再用 HFP 回调冒充 iAP2，也不再把蓝牙 RFCOMM 注册参数当作已知常量。

## 1. 为什么换方向

根据你原厂 MU1005 文件的真实 SHA256 校验（仅私有本地使用，未放入 ZIP/GitHub）：

- `/eso/bin/apps/bluetooth`，SHA256 `7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272`，确实包含 `bluetooth.enableIap`、`iapEnabled`、`SERVICETYPE_IAP2`。这证明配置属性和服务类型名称存在；**并不能证明该服务在运行**。
- `ipod-drvr-iap2.so`，SHA256 `42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf`，包含 `BluetoothiAP2Link`、`BluetoothTransportComponent`、`WiFiTransportComponent`。它们证明原厂驱动具备相关消息处理代码，**并不能证明蓝牙 RFCOMM 驱动完成绑定**。
- R98 原厂 `btstack` 插桩方案在生产环境仍然缺 iAP2 真实线程/服务入口，不能靠 HFP 模拟证据绕过。

**路线决定：先尝试原厂软件已提供的 iAP2 功能分支；如果最终发现不带相应 transport，才回退到 R98 的自建 RFCOMM 路线。** 不是把 MHI2Q/QC 的现成二进制拿来直接安装到 AU57x。

## 2. 新增工具——确实实现的内容

`tools/r99_stock_iap2_route.py` 一次读取 **两个真正的源配置**（蓝牙 JSON、`iap2.cfg`），要求用户自带精确 SHA256，同时核验上述两个同固件原厂 ELF：

- 蓝牙 JSON：只接受唯一的 `enableIap=false`、`bluetooth.enableIap=false` 或规范嵌套键；只把这个值改为 `true`。未知结构、重复键、已经开启、真假值类型不对，一律拒绝。
- iAP2 INI：只接受唯一一个活动的 `[bluetooth] enable=0/false`，或唯一一个完整的 `#[bluetooth]` 注释模板；生成 `enable=1`。其他 Wi-Fi、USB 等设置不动。活动 section 重复、未知值、没有明确模板，一律拒绝。
- **两个文件必须一起验证成功才生成**。输出字节级原文件、标为 `STAGED_REVIEW_ONLY` 的新文件、原始与改后哈希、恢复说明和明确的 `installable=false` 清单。不写车机分区，也不替换输入原件。
- `tools/r99_verify_stage.py` 是独立的二次检查：比对四份文件 SHA256，按原文件重新计算允许的唯一改动，检查无新文件、无符号链接，拒绝被改成“可安装”的清单。

## 3. 实际验证

- 本轮 Python 用例 **23/23 PASS**，包括实际文件读写、两个 JSON 结构、注释版 ini、重复/篡改/错误哈希、路径劫持、独立复核。
- 在私有测试环境用上述 **两个真实原厂 ELF** 核验通过。
- 用`tests/fixtures/R99_DEMO_NOT_OEM_*`示例配置走完整的 CLI 路径：双配置生成→校验→恢复原文件对比，PASS。示例配置**不是你的真实车机配置**，不得作为实车配置使用。
- R98 原有 iAP2 TCP 交接和 43 C 模块未删除，保留为备选，不因此被认定实车成功。

## 4. 真实原车为什么还不能直接装

当前工程中**没有确切的原车蓝牙配置文件及原车 `/etc/mm/iap2.cfg` 的原始字节**，也还没证明原厂 `bluetooth.enableIap` 为 true 后会注册 iAP2 RFCOMM 服务。所以本轮做的是**完整可复核的双配置离线生成器**，而不是已生成属于你车辆的真实修改配置。更不能把这说成无线 CarPlay 已连接。

还缺系统实际加载这两份配置的位置/格式，原厂蓝牙到 mm-ipod/driver 的 transport 绑定，以及最终 Apple MFi 和 AirPlay 认证。你现在无需上车；已有文件可继续离线适配，最后一次装车才验证原车真实运行。

## 5. 使用（仅电脑环境）

```bash
# 用真实原厂文件审计时，另外传入两份真实配置；必须由使用者明确提供原始 SHA256。
python3 tools/r99_stock_iap2_route.py \
  --bluetooth-config /path/to/original_bluetooth.json \
  --iap2-cfg /path/to/original_iap2.cfg \
  --bluetooth-sha256 <ORIGINAL_BLUETOOTH_CONFIG_SHA256> \
  --iap2-sha256 <ORIGINAL_IAP2_CFG_SHA256> \
  --original-bluetooth-elf /path/to/MU1005_original_bluetooth \
  --original-iap2-driver /path/to/MU1005_original_ipod-drvr-iap2.so \
  --out /tmp/R99_OFFLINE_REVIEW
python3 tools/r99_verify_stage.py /tmp/R99_OFFLINE_REVIEW
```

工具的结果目录可以直接检查，但**不能当作 SD 卡安装包**。提示 `DENY` 时表示输入信息不够确定，不允许盲目继续。原厂 `bluetooth`、驱动文件绝不能上传到公开 GitHub。

**交付状态：该项离线工具完成；原车服务激活 / MFi / AirPlay 尚未完成；不允许装车。**
