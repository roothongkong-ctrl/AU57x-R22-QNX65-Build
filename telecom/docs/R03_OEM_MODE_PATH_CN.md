# R03｜原厂 NAD 网络模式请求链（真实文件只读复核）

车辆：2017 Audi A6L C7，MHI2_CN_AU57x_K1006_2/MU1005。

## 真正验证的原始文件

此轮通过用户历史上传的原始 ELF 只读检查，**没有向公开 GitHub 仓库上传原厂二进制**：

- `nad`：845158 字节，SHA256 `ed7202ec171deb978f07d71efcad836707be3cb9a1104ced98c51709649d3e21`。
- `telephone`：1458120 字节，SHA256 `63a3bdcf0b681991efae19cec87583e33423d0623ae7386510f6485815c88f52`。
- `connectionmanager`：758371 字节，SHA256 `018cca040ad6136a09bbd41bcc9de80908cd6a5b94e8d9f1ed31076a23ecb1cb`。

这是**本地原件审查证据**，GitHub Actions 默认只运行开源代码/合成夹具，不能把其绿勾说成“云端已运行车机原件”。

## 新增 ARM 指令级结论

在原厂 `nad` 的相同固件 ELF 中定位到：

| VA | 原始 ARM 指令（十六进制） | 证明范围 |
|---|---|---|
| `0x18B104` | `E1A05001` | `mov r5,r1`：请求指针进入 r5 |
| `0x18B160` | `E5951010` | `ldr r1,[r5,#16]`：从请求结构 `+0x10` 取参数 |
| `0x18B168` | `E251B001` | 用 1 比较该参数 |
| `0x14CD3C` | `EB00F8EA` | 直接 BL 至 `0x18B0EC` |
| `0x15625C` | `EB00D3A2` | 直接 BL 至 `0x18B0EC` |
| `0x1731C4` | `EB005FC8` | 直接 BL 至 `0x18B0EC` |

此前确认与这一路径关联的 `AT+COPS` 模板仍是：

- `mode=1` + 手动：`AT+COPS=1,2,%s,0`（手动 GSM AcT）；
- `mode≠1` + 手动：`AT+COPS=1,2,%s`（未明确 AcT）；
- 自动分支有 `AT+COPS=0,2,,0` 与 `AT+COPS=0,2`，**对真实模块自动注册的影响未验证**。

**关键结论**：在 NAD 中可证明取值来源是传入的请求结构字段，而不是这段选网处理程序在内部按中国电信硬编码生成。下一层须追踪请求的填充和 `telephone` 的 `NADServicesProxy` 上游参数。三个直接调用点并不证明当前实车会使用哪一个，也不说明实时 `mode` 取值。

此处的 OEM 业务字段 `mode`，不能与标准 `AT+COPS` 指令的第一个 `<mode>` 参数混淆。

## 当前对外发布阻塞

- 尚未证明电信 SIM 的请求 `mode=1`；
- 尚未确认车机真实 modem 型号、LTE 频段兼容或运营商注册拒绝码；
- 未制作 carrier-scoped QNX 修复补丁，也未验证补丁对中国移动无影响；
- 不能生成实车安装包。GitHub release gate 必须保持 `BLOCKED`；
- 下一步需要可复验的上游赋值证据，或一次真正安全的只读运行时观测。

本项目代码：`telecom/tools/mode_provenance.py`，云端使用合成 ARM 夹具检查，另对用户授权的原厂文件执行过一致性人工核对。

## R03b｜向上追踪到 telephone 的 NADServicesProxy 转发

再次使用用户已上传、SHA256 匹配的原厂 `telephone` ELF 对照 ARM 字节：

- `0x1F5360`：相关转发函数入口（标准 ARM 调用约定）。
- `0x1F536C`：`mov r8,r2`，保存运营商参数。
- `0x1F5370`：`mov r10,r3`，保存 `mode`。
- `0x1F538C`：`mov r7,r1`，保存手动选网标志。
- `0x1F53F0` / `0x1F53F4` / `0x1F53F8`：恢复 `r1/r2/r3`，其中 `r3` 直接由 `r10` 恢复，没有在本段内改动 mode。
- `0x1F5414`：`blx r12`，由代理虚函数接口转发请求。

**结论：** 该 `telephone` 转发层不会把 mode 改成 1；若电信卡真的误选 GSM，需要继续追查该转发函数的上层调用者或 NAD 入站请求解析。这里未证明任一 SIM 真实传参，也未证明模块有 LTE 注册能力。

审计代码：`telecom/tools/telephone_mode_bridge.py`，本地原始 ELF 八处全部匹配；GitHub 云端通过合成数据回归测试，不包含原件。
