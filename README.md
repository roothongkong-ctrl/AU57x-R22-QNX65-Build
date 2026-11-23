# R101：MU1005 原厂 RFCOMM 通道范围与注册参数审计（禁止装车）

**本轮新增**：确认原厂 HFP=7、SIM=4 都流向同一原厂注册函数，并写进原厂通道对象；此参数看起来更像 SCN，而非已证实的 profile ID。修复原 C 代码错误的 1～6 通道限制为标准 1～30，将验证的 SCN 值冻结为一次会话快照，防止参数前后变化。详见 `docs/R101_RFCOMM_SERVER_CHANNEL_CN.md`。

**状态**：只读原厂静态 ELF + 可编译 C 源码、离线测试；没有确定原厂 iAP2 真正注册参数/所有者，**不准装车**。

---

# AU57x Wireless CarPlay – R100 reversible factory-to-graft branch

Status: **SOURCE ONLY / NO INSTALL / NOT VEHICLE VERIFIED**.

**R100 deliverables**: Original MU1005 exact-ELF feature audit; confirmed divergences from other MHI2 firmwares; native C fail-closed factory/graft selection; new guard on QNX R98-to-R45 BlueSDK attach entry; 44-module source compile and regression tests. R99 dual config generator remains as factory branch and R97/R98 remain experimental graft branch; both still require live OEM evidence.

Chinese explanation: `docs/R100_FACTORY_TO_GRAFT_CN.md`. Private OEM files are NOT included.

Reproduce with original files on a private machine:
```sh
R100_ORIGINAL_DIR=/path/to/MU1005_original_ELFs bash ci/host_r100_route_audit.sh
python3 ci/host_r100_continuous.py
```

The result `r100_result/R100_ORIGINAL_MU1005_ROUTE_AUDIT.json` contains hashes and offsets only. It must not be treated as an installation endorsement. **Do not disable MFi, modify the original `/eso/bin/apps/btstack`, replace factory `bluetooth`, or install any review ZIP to the car.**

---

## R102：直接复用上游 GPL 源码，不再重复造轮子

现已从 `luka-dev/mib2q-carplay-rgi` 精确引入 `wireless/graft/eir.c`，保证 1691 字节原文件与上游 Git blob 完全相同，并编译到 AU57x 的独立审计入口 `au102_eir_advertisement`。只在原厂 btstack、iAP2 RFCOMM 和 SDP 都已确认的情况下允许广播，保留有线 CarPlay 优先。测试：`bash ci/host_r102_upstream_reuse.sh`。

详情：`docs/R102_OPEN_SOURCE_REUSE_CN.md`。**该源码只证明一个通用组件可移植，不证明无线 CarPlay 已完成；禁止装车。**
