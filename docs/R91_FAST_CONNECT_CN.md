# R91：快速连接实验——放宽一个门禁，不伪称已经无线可用

## 发生了什么

用户明确希望优先尝试连通。R90 的 MU1005 原厂 `RF_RegisterServerChannel` 第三参数尚未准确确定。原厂 HFP 使用 7、SIM 使用 4；这些只是实际参数值，**不能证明这个参数就是“iAP2 profile”或应该使用哪个数字**。对函数本体的 ARM 静态分析确认参数传入并存入 RF 对象，但不能替代实车运行证据。

R91 新增 `rf_arg_experiment.[ch]`，并将 `native_binding.c` 调用参数选择独立为可测试单元。正常模式仍只有原厂明确验证的值才能继续；`AU57X_FAST_CONNECT_EXPERIMENT=1` 时在未知情况下允许试 `0`（参考 MHI2Q 工作方案，**未证明与 MU1005 兼容**）。如已存在原厂验证的参数，永远优先采用验证值。

## 对用户意义

- 一旦具备真正的原厂进程加载/回调和 iAP2 daemon，这个原来在 unknown 时必定拒绝的地方可以尝试继续，缩短“首连”实验路径。
- R90 实际 iAP2 SYN/LSP/ACK 与 MFi CONTROL 仍继续执行；苹果认证不能被模拟绕过。
- **本包仍然不包含能在车上安装并自动接通的启动器。** `LD_PRELOAD`/原厂 btstack 启动生命周期不在这次修改范围内。强行将此 `.so` 放入车机不能自动产生无线 CarPlay。
- 不附带原厂二进制、钥匙或者第三方 MHI2Q 编译文件。

## 测试

```sh
bash ci/host_rf_arg_experiment.sh
bash ci/host_wcp_r45_remote.sh
bash ci/host_wcp_r45_remote_mfi.sh
```

## 当前最快的后续实际突破

原厂 `btstack` 已经存在可注册 RFCOMM 的候选 ARM 地址，但需要在实车进程生命周期里实际调用并收到真机事件。**只有真实车机能证明这点，电脑云端只能证明编译和状态机。** 以 2.4GHz 为默认无线承载，不等待未证实的 5GHz；有线优先保留。

R91 代码不作为 SD 刷机程序；`installable=false`, `safe_to_flash=false`, `vehicle_verified=false`。
