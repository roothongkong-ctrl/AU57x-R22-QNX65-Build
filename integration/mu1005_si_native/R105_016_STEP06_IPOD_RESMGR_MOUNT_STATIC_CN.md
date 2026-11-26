# R105.016｜第 06 步：mm-ipod Resource Manager mount 调用链闭环（静态）

日期：2026-11-26  
目标：MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32  
状态：OEM_STATIC_CALLCHAIN_CONFIRMED / HOST_METADATA_GATE / NO_INSTALL

## 结论

本轮首次把 `ipod-drvr-iap2.so` 对 `ipod_resmgr_mount` 的调用，直接解析到同版本 `mm-ipod` 导出的实现，并进一步定位到 QNX `resmgr_attach` 的实际调用点。

这证明：**iAP2 driver 调用 mm-ipod 宿主导出的 mount helper，由 mm-ipod 地址空间中的实现执行 Resource Manager 注册。**

但本轮没有证明车辆运行时 PID、mount 后的实际节点名、该节点是否就是 DIO 配置中的 `/dev/ipod0`，也没有证明 RFCOMM 已经接入该节点。

## 1. 固件样本身份

| ELF | 大小 | SHA256 |
|---|---:|---|
| mm-ipod | 40,297 | `4b87cd2d41ddcee2f38800fe8afe8b4b340e4f23e1d86c7e97af303522aa15d09` |
| ipod-drvr-iap2.so | 739,925 | `42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf` |
| dio_manager | 690,843 | `e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c` |

原厂 ELF 只在用户提供的私有交接包内分析，没有上传到公开仓库。

## 2. Driver 侧调用点

`ipod-drvr-iap2.so`：

- `iap2_start_features`：VA `0x5fdc`
- `ldr r9, [r0, #0x14]`：VA `0x5ff0`，机器码 `0xe5909014`
- `mov r0, r9`：VA `0x63c8`，机器码 `0xe1a00009`
- `mov r1, #1`：VA `0x63cc`，机器码 `0xe3a01001`
- `bl 0x4cd0`：VA `0x63d0`，机器码 `0xebfffa3e`

该 ELF 的 `.rel.plt` 中，`ipod_resmgr_mount` relocation 位于 `0x000a9a14`，对应 PLT stub `0x4cd0`。因此这不是根据字符串猜测，而是一个可复核的真实外部函数调用。

`iap2_start_features` 又由 `link_handle_iap2pkt` 在 VA `0x13628` 调用；调用前检查状态字段低两位是否等于 3。这里只记录控制流，不把状态 3 擅自解释为 Apple MFi 成功或完整 CarPlay 会话。

## 3. Host 侧函数与 Resource Manager

同一版本 `mm-ipod` 导出：

- `ipod_resmgr_mount`：VA `0x103ad0`，大小 512 bytes
- `ipod_main`：VA `0x103d8c`

`ipod_resmgr_mount` 的入口保存：

- `r6 = r0`（context）
- `r7 = r1`（mount mode）

当 `r7 != 0` 时，函数从 context 的 `+0x04` 读取自定义路径指针，从 `+0x20` 读取 dispatch 指针，随后把该路径交给 `resmgr_attach`。

当 `r7 == 0` 时，函数调用 `rsrcdbmgr_devno_attach` 获取设备编号，并通过格式串 `%s%d` 与 `/dev/ipod` 生成默认路径；之后同样调用 `resmgr_attach`。

关键调用：

- `ipod_main` 在 `0x104168` 调用 `ipod_resmgr_mount`，参数为全局 context、mode 0。
- `ipod_resmgr_mount` 在 `0x103c94` 调用 `resmgr_attach`（PLT `0x101c84`）。
- `mm-ipod` 的帮助字符串包含 `-m <mountpath>`，说明设备路径可配置，默认值描述为 `/dev/ipod#`。

因此，默认 `/dev/ipod#` 的生成机制已静态定位；driver 调用 mode 1 时使用 context `+0x04` 的自定义路径。**尚未证明这个自定义路径的运行时值。**

## 4. 对 /dev/ipod0 的影响

DIO 的配置样本使用 `/dev/ipod0`。本轮发现说明它与 mm-ipod 默认节点命名机制相容，但不能直接认定：

1. 当前车辆的实际设备编号一定为 0；
2. driver mode-1 自定义路径一定也是 `/dev/ipod0`；
3. DIO 实际打开的 fd 对应哪个 mount instance；
4. 该节点已经接收蓝牙 RFCOMM 字节。

这些仍需从配置读取、context `+0x04` 的赋值来源和运行时只读元数据闭环。

## 5. 新增自动门禁

新增 `docs/R105_STEP06_RESMGR_MOUNT_STATIC_AUDIT.json` 与 `tests/test_r105_step06_resmgr_mount_audit.py`，固定检查两个 OEM ELF 的精确 SHA、driver 调用参数、PLT relocation、mm-ipod 导出函数、QNX `resmgr_attach`、默认路径与自定义路径的区别，并确保 RFCOMM/MFi/车辆运行时门禁保持关闭。

该门禁只校验本轮私有静态审计记录，不会伪装成 CI 对私有 OEM ELF 的独立重分析。

## 6. 下一步

1. 追踪 `ipod_main` 中 `-m` 解析结果写入 context `+0x04` 的完整分支，区分启动默认路径和显式覆盖路径。
2. 追踪 driver mode-1 mount context 的构造者，确认 `+0x04` 路径的来源。
3. 将 DIO 配置 `iap2.device` 到 `iap2_connect(path,...)` 的参数流与具体 mount instance 对齐。
4. 再追踪 Resource Manager 的 connect/read/write/devctl handler，和 `ipod-drvr-iap2.so` 的 transport send/receive 层对齐。
5. 保留原厂 `usbdevice` 传输、有线 CarPlay、MFi 原厂认证；不替换插件、不伪造认证。

**本步完成的是 mount owner 与静态调用链，不是无线链路。**

`vehicle_verified=false` / `mfi_apple_verified=false` / `installable=false` / `safe_to_flash=false`。
