# AU57x / MU1005 纯软件无线 CarPlay：R32 云端审查工程（不可装车）

基线：2017 Audi A6L C7，MHI2_CN_AU57x_K1006_2 / MU1005，QNX 6.5 ARMv7。

**警告：本包仅为公开源码、自动测试和静态原厂研究结果，无实车可刷写载荷，无 Apple MFi 密钥。GitHub 构建成功不代表原厂无线链路已打通。**

## 云端可完成的范围

01 交接已完成；02 原厂 ELF 静态证据审计已经覆盖 SI/DIO/iAP2/AirPlay/高层 Bluetooth/mm-ipod/USB 插件，但 RFCOMM→iAP2 真实运行端点尚未证明；03 19 C 模块 QNX 原生编译及 QEMU 测试此前通过；10 现在包括 fail-closed 的12步台账、断言伪造拒绝、有线抢占与会话模型、OEM 授权拒绝、50,000 CSM fuzz样本、SD只读包路径/摘要负向测试。04–09 的原厂授权/无线射频/音视频控制仍没有真实联调，11无实车写入表或恢复路径，12未开始。

查看 `docs/CLOUD12_LEDGER.json` 和 GitHub Actions 的 `AU57x-cloud12-evidence-NO-INSTALL` 产物。工具只复核**静态研究记录**与现有源码测试，不接管原厂进程，也不会自动安装。

## 原厂缺口，不应伪报“完成”

1. Bluetooth ASI服务对象存在不等于其运行时注册/回调归属已证实。
2. `mm-ipod` 的插件动态加载和`/dev/ipod` 字串不等于存在蓝牙 iAP2 传输插件。
3. 需相同设备身份下真正完成蓝牙iAP2 → 正常 MFi认证 → WLAN关联 → Bonjour/AirPlay → 原厂 SI/DIO 媒体控制，并保留原厂有线优先。
4. SD事务只具备离线审查机制，禁止把任何主机fixture视为可装车安装包。

## 云端回归

- `python3 tools/cloud12_gate.py`
- `python3 -m unittest discover -s tests -p test_cloud12_gate.py -v`
- `python3 -m unittest discover -s tests -p test_sd_package_audit.py -v`
- `bash ci/host_handoff.sh`
- `bash ci/host_oem_guard.sh`
- `bash ci/host_csm_fuzz.sh`
- `bash tests/run_bt_host.sh`
- `git archive` 生成只含公开源码的可追溯 ZIP，绝不包含真实 OEM 二进制。

### 法律与来源
代码包含 GPL-3.0-only 标记，详见仓库 `LICENSE` 及 `NOTICE_THIRD_PARTY_CN.md`。归属与许可证应随源码一起保留。
