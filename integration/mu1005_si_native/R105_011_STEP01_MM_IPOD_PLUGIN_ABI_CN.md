# R105.011｜第 01 步：mm-ipod 外层 Transport 插件 ABI 静态解析

日期：2026-11-26
目标：MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
固定计划：22 步中的第 01 步
状态：STATIC_LAYOUT_MAPPED / HOST_GATE_ADDED / OEM_CALL_SIGNATURES_PARTIAL / NO_INSTALL

## 本步完成的工作

复用既有 R24/R31 原厂只读审计，不重复提取或重复计算 SHA：
- mm-ipod：40,297 bytes，SHA256 `4b87cd2d41ddcee2f38800fe8afe8b4b340e4f23e1d86c7e97af303522aa15d09`
- ipod-transport-usbdevice.so：SHA256 `e1743821b166fcd93992b45fc7064ad14763ee8a4445478e9aa36e696f2f6e95`
- bluetooth：SHA256 `7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272`
- 既有 OEM ELF 静态断言：22/22 PASS（R24 历史审计记录）。

### 1. Loader 入口

已确认的 mm-ipod 加载结构：
- 导出入口：`device_transport_init`，VA `0x103510`
- 插件模板：`ipod-transport-%s.so`
- 动态加载：`dlopen` / `dlsym`
- driver 模板：`ipod-drvr-%s.so`
- 原厂设备服务相关字符串：`/dev/ipod`
- 帮助文本列出：`usbdevice,usbhost`

注意：字符串/导出符号证明存在模块化加载机制，但不能单独证明命令行参数如何选择插件、实际加载了哪个插件、失败时如何回退或运行时进程身份。

### 2. Transport module 对象布局

原厂 USB 参考模块：
- `ipod_module`：VA `0x3ec0`
- ABI version：`0x0200`
- transport callback table：VA `0x40b8`
- callback prefix：48 bytes（ARM32 32-bit pointer slots）
- module structural object：36 bytes

### 3. 七个回调槽位

| 偏移 | 已知槽位 | USB donor VA | 当前能确认的含义 |
|---|---|---:|---|
| +0x14 | init | 0x1934 | 初始化回调入口；完整参数/返回码语义未闭环 |
| +0x18 | link_params_get | 0x1874 | 获取链路参数；结构体字段/长度待恢复 |
| +0x1c | type_get | 0x18bc | 获取 transport 类型；枚举值及能力映射待恢复 |
| +0x20 | caps_get | 0x18cc | 获取能力；USBHOST donor 此槽可为空，需按插件能力处理 |
| +0x24 | audiopath_get | 0x1910 | 获取音频路径；输出对象布局待恢复 |
| +0x28 | data_send | 0x1560 | 数据发送入口；buffer/length/同步语义待恢复 |
| +0x2c | data_recv | 0x1228 | 数据接收入口；buffer/length/回调语义待恢复 |

callback table 的 +0x00 为 transport name，+0x04..+0x13 是四个在两个原厂参考插件中均为零的槽位。它们目前只能标为“参考样本中观察为零”，不能断言 ABI 永久保留或可随意使用。

module object：
- +0x00：name relocation
- +0x04：ABI version
- +0x08..+0x1f：六个 opaque word（两个参考样本为零）
- +0x20：private descriptor pointer

上述结构已由 `project/source/src/cp_ipod_module_layout.h` 表达为 **structural view**。它不是可直接注册的新插件，也没有虚构函数原型。

### 4. 本轮新增自动门禁

新增 `tests/test_r105_step01_plugin_abi_layout.py`，并接入 `.github/workflows/r105-iap2-host-review.yml`：
- 检查七个 callback 槽位与偏移；
- 用宿主机 C 编译器执行 `_Static_assert`，验证 callback table 48 bytes、module object 36 bytes 和各字段偏移；
- 检查审计边界仍明确标注无线插件、RFCOMM owner、车辆安装均未证明。

这属于主机结构布局检查，不是 QNX runtime，也不替代原厂 ELF 反汇编。

## 本步结论

**第 01 步的“外层 ABI 结构图/槽位映射”已完成。** 已有 22/22 原厂静态审计结果可复用，本轮新增独立布局编译门禁。

仍未完成、且不冒充已知的内容：
1. 七个回调的完整 C 函数签名、寄存器/栈参数与返回码。
2. loader 对 init/destroy 的精确调用顺序和失败回滚。
3. owner/private descriptor 的分配、释放及线程约束。
4. 当前车辆实际加载的 transport 名称和运行时 PID。
5. Bluetooth RFCOMM 与此外层插件 ABI 的真实连接关系。

这些细项将在第 02 步与原厂 driver、USB donor 交叉引用时继续核对。**不因 ABI 表格完成而编写或安装无线插件。**

验收：
- static layout：PASS
- host compile assertions：新增，等待 CI run
- OEM runtime：NOT TESTED
- Bluetooth RFCOMM linkage：NOT PROVEN
- vehicle verified：false
- installable：false

## 第 01 步自动门禁实测（2026-11-26）

R105 workflow run `45757541072` 已完成 SUCCESS：
- R103 上游协议层和 AU57x bridge host 回归通过；
- C RFCOMM frame framing/disconnect 测试通过；
- R105 owner nonblocking 五项源码门禁通过；
- 本步 ABI 布局四项门禁通过，其中宿主机 C 编译器实际编译并执行了结构断言；
- scope artifact 明确为 HOST_ONLY / OEM runtime NOT TESTED / installable=0。

本轮曾因测试脚本的字段匹配表达式过严而出现一次失败；已修正为直接字段/偏移断言，随后同一工作流完整通过。最终验收以成功 run 为准。

**固定 22 步进度：01/22 的结构映射验收完成。** 七个函数的真实参数签名、返回值和完整 loader lifecycle 尚未闭环，不会写成已知 ABI；它们在第 02 步与原厂 driver 交叉引用时继续确认。
