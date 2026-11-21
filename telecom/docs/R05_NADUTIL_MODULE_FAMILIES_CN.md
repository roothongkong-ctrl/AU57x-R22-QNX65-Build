# R05｜原厂 NADUtil 的华为与 Cinterion 双模块分支

适配：2017 奥迪 A6L C7，MHI2_CN_AU57x_K1006_2/MU1005。

## 已核实原厂样本（私有原件，仅本地只读）

- NADUtil：19,296 bytes，ELF32 ARM QNX。
- SHA256：67755ff17740728290ce758e80d57904cb6b105bbf4f780d803e97ab5f60f1d2。
- 原厂内容经固定偏移和字节核查，GitHub 仅公开检测脚本和非敏感字面信息，**未上传原厂程序**。

## 真实原厂代码线索

| 文件偏移 | 静态内容 | 能证明的范围 |
|---|---|---|
| 0x3AB0 | ME909Au-821 | Huawei 型号识别分支存在 |
| 0x3A08,0x3D68 | ATI1、ATI51（以 CR 结束） | 原厂通过串口做硬件信息查询 |
| 0x37B4 | /ramdisk/huawei_me909au_821 | 华为相关维护路径 |
| 0x3FF8,0x429C | ALS6A、AH6A | Cinterion 相关识别分支也存在 |
| 0x42C8 | /ramdisk/cinterion | Cinterion 相关维护路径 |
| 0x3948 | ATPort /dev/serusb3 already locked | AT 串口可能被占用、加锁 |
| 0x421C | /dev/serusb4 | 另一设备端口路径 |

**不能由样本含有 ME909Au-821 字符串推断实车实际模块就是该型号。** 同一个原厂文件还支持 Cinterion ALS6A/AH6A。曾出现的模块型号日志仍须和当前启动时驱动分支绑定。

原厂 NADUtil 会打开/锁定串口并发 ATI 命令，因此**不允许将该工具直接运行在车机当作只读探针**，也不要争抢正常 NAD 服务的串口。

## 对今后修复的影响

- 不把 qemu-mhi2 模拟的 Cinterion ALS6 误认为 AU57x 必然采用的模块。
- 不把公开 ME909s-821 的规格套用于 ME909Au-821；准确 LTE 频段仍需真实硬件型号。
- 继续追踪电话服务上游 mode 参数的赋值与 CT LTE 注册拒绝码。
- 原厂程序、安全认证信息与任何 SIM 身份标识都不得进入公共 GitHub 仓库。

**R05 当前状态：原厂工具支持多模块族已验证；实车模块型号和电信实际注册失败原因仍未确定。**