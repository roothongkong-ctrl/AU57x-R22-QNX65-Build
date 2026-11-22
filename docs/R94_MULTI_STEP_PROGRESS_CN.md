# R94：连续推进多项，而不是每轮只做一步

基线：2017 奥迪 A6L C7 PA，`MHI2_CN_AU57x_K1006_2 / MU1005`。原始二进制不包含在公开工程内。

## 本轮完成并可离线验收的工作

1. **R93 QNX 原生 CI 最终验收：完成。** 原分支 GitHub Actions run `45036838095` 显示 `success`；它验证 QNX ARMv7 编译、ELF、QEMU dlopen，仍未启动原厂蓝牙无线电。
2. **MU1005 原厂启动器的静态入口审计：完成。** 使用用户既有的真实 `connectivity_launcher`（SHA256 `08896149dcdcc2bc5a6b22ce78682ad5949fbeb8fdc0fb1249162861e40914b8`）证明其为 ARM32 动态 QNX executable，具有 `OSAL ProcessTNG::setExecutable`、`connectivity.json`、`JSONReader`、`StartModule` 引用。新增 `tools/r94_launcher_audit.py`，对型号、SHA、标志做闭环与 4 组变异测试。
3. **启动配置可回退生成器：完成（离线模板，不代表已通过真实车机配置兼容性）。** 新增 `tools/r94_launcher_patch.py`，可从明确识别且只含一个 `btstack` 的 JSON 配置生成：`connectivity.original.json` 原样备份、`connectivity.STAGED_REVIEW_ONLY.json` 修改版、`au57x_btstack_probe_entry.sh` 同进程 `exec` 包装脚本、SHA256 溯源清单。所有其他模块、参数、重启/依赖条件原样保留。未知配置结构、不是精确原厂路径、双重 btstack、配置哈希不符或输出已存在时拒绝生成；无需任何汽车网络访问。
4. **已有跨进程、iAP2/MFi、Wi-Fi、AirPlay 控制门禁、SD 恢复基础回归：通过。** 12 组 shell 测试包含真实本地 TCP 字节流和明确标识的假证书；不等于真实苹果授权。

## 尚未完成、仍然是真正阻塞的工作

- 当前用户原车的 **准确 `connectivity.json` 文件字节和路径**没有在 R94 内得到确认；生成器只支持明确静态识别的结构，当前不存在可直接写回汽车的补丁。
- 工厂监督程序是否能让 `LD_PRELOAD` 传入原车 `btstack`、是否受权限模式影响，仍未经车上确认。
- 真正原厂 BlueSDK RFCOMM 回调线程/owner 与 `mu_bt_native_attach` 还未在车机生命周期中接通。实验性 RF arg=0 不是已证明的 iAP2 profile。
- 真实 iPhone 原厂 MFi、2.4GHz Wi-Fi、AirPlay 视频/声音仍没有实车证据。
- 在完成全套离线可推进的开发前，不请求用户上车；本包**不是 SD 自动安装包**。

`installable=false`; `safe_to_flash=false`; `vehicle_verified=false`。

## 离线使用样例

```sh
python3 tools/r94_launcher_audit.py --elf /private/MU1005/connectivity_launcher --out /tmp/r94_launcher_proof.json
python3 tools/r94_launcher_patch.py --config /private/MU1005/connectivity.json --out /tmp/r94_preview --expected-sha256 ORIGINAL_SHA256
bash ci/host_r94_launcher.sh
```

`/tmp/r94_preview` 是电脑侧预览，**绝对不能直接刷入车机**；生成结果中记录 `factory_supervisor_environment_inheritance_proven=false` 和 `vehicle_verified=false`。
