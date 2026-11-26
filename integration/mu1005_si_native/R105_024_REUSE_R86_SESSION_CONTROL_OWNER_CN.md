# R105.024｜复用 R86 原厂 AirPlay SessionControl owner，收敛到真实命令入口

日期：2026-11-26
目标：MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
基线：原厂 dio_manager / libairplay.so / smartphone_integrator 的 R86 精确 SHA 固定审计；R105.021–023；inner_iap2 当前源码
状态：EXISTING_OEM_DELEGATE_REUSED / NEXT_DISASSEMBLY_TARGET_IDENTIFIED / NO_INSTALL

## 1. 本轮纠正的历史遗漏

R105.023 初稿把 AirPlay receiver callback owner 写成尚未定位。进一步检索用户 Library 后发现 R86 已完成同一 MU1005 原厂 ELF 的两级 delegate 注册审计。本轮已经把 R86 结果追加到 R105.023，撤销“尚未定位 delegate owner”的过宽说法。不得再次扫描 server/session delegate 结构或重复要求用户上传这三个 ELF。

R86 的准确证据：
- DIO 在 0x15DF68 调 AirPlayReceiverServerSetDelegate（PLT 0x112514）；传入 60 字节 server delegate，libairplay 0x1CEEC 将其复制至 server +16。
- DIO 的 s_sessionCreatedCallback @0x159374 在 0x159448 调 AirPlayReceiverSessionSetDelegate（PLT 0x11270C）；传入 56 字节 session delegate，libairplay 0x1FB44 将其复制至 session +20。
- session delegate offset +16 是 s_handleSessionControl @0x1594EC，GOT slot 0x1A7A3C。
- session delegate 其余已识别槽包括 sessionFinalized、sessionCopyProperty、sessionNotifyProperty、sessionModesChanged、sessionRequestUI、duck/unduck、audioTornDown、airplayError。
- 两级 delegate 所有非空槽均通过原厂 R_ARM_GLOB_DAT relocation 指向 CDIOManager 符号。

原厂 SHA256：
- dio_manager e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c
- libairplay.so 193a4fd9101ec2aa05e7159cfa307b96500810d379ca74a194f172adc13a46b5
- smartphone_integrator 2c61e7a17d5c77df2f97023284b948b66dc4052010acad74be9bfa63b339de0b

私有原厂二进制不上传公开仓库。上述值和槽位来自 R86_OEM_DELEGATE_AUDIT.json 与 R86_ORIGINAL_OEM_DELEGATE_ABI_CN.md。

## 2. 现在真正需要反汇编的函数

下一入口已收敛为：

- `dio::CDIOManager::s_handleSessionControl`，DIO VA `0x1594EC`。
- 该函数被原厂 AirPlay session delegate 的 offset +16 槽引用。
- 它是现有 session control callback，不等于已经证明 iAPSendMessage 命令会进入这里。

需要从原始 MU1005 dio_manager 的函数体恢复：
1. ARM 函数边界、入口寄存器与栈参数；
2. 是否接收 session 引用、command/control object、reply/error output；
3. 对象类型检查和字典/CFData访问路径；
4. 是否存在 command dispatch、字符串/CFString 比较或子命令分支；
5. 原厂处理是否同步执行、回调返回码及是否必须继续调用既有 handler；
6. 所属线程/dispatch queue、锁持有情况和 session 生命周期约束；
7. 是否有调用者在 encrypted socket 之前完成解密与 plist decode，或此 callback 只处理 AirPlay session control enum。

## 3. 对 inner_airplay 的接入判断

当前 `inner_airplay.h/.c` 已提供：
- `mu_inner_ap_bind()`：固定原厂 libairplay handle、session、epoch 和 owner proof；
- `mu_inner_ap_control()`：接受调用者传入的已解码 dictionary，识别 type=iAPSendMessage、提取 params.data，并交给 inner_iap2；
- 对非 iAPSendMessage 返回 2，约定调用者继续原厂处理；
- 错误 session、错误 epoch、结构错误或 owner 失效时 fail closed。

但 `mu_inner_ap_control()` 目前只是一个适配函数。当前仓库没有证明它被 s_handleSessionControl 调用，也没有证明 s_handleSessionControl 收到的参数形状与 CFDictionary 相同。不能仅凭回调名字、delegate slot 或 host fake CFL test 宣称接通。

因此不能在缺少 s_handleSessionControl 函数体/参数反汇编时写生产 hook，也不能替换原厂 callback 或伪造其返回值。

## 4. 与 Event Channel pump 分离

AirPlay session delegate 与 AirPlay event socket reader 是不同层次：
- SessionControl callback 是 DIO 注册给 libairplay 的会话级回调槽；
- Event Channel pump 负责事件 socket 单读者、stock/bridge request wire-order queue、回复归属与缓冲。

R86 证明 delegate registration，不证明 socket fd owner、select/poll/read/write interposition 或 no-CSeq queue。参考 MHI2Q 项目的 pump 算法不能直接按其函数地址、GOT slot 或 QNX socket fd 布局移植。

## 5. 已经做的工程变更

提交 aa89410fe391651474df4620e300e22d04cfaa39 将 `inner_iap2/test_airplay.c` 加入 R105 host workflow：使用 host fake CFL library 编译并执行 AirPlay envelope adapter 回归，覆盖发送封装、返回包处理、非目标命令交还、错误 epoch、busy、send failure 与 owner loss。workflow path filter 已加入 `integration/mu1005_si_native/inner_iap2/**`。

注意：本轮查询时 GitHub 尚未返回 aa89410 对应的 Actions run，因此该新 CI step 尚不能报告 PASS；已有旧 run 的成功结果不包含这个新步骤。需要以该 commit 对应的新 run 为准。

## 6. 当前状态与下一步

| 项目 | 状态 |
|---|---|
| DIO AirPlay server delegate 注册 | R86 静态证据 PASS |
| Session delegate 注册与 10 个回调槽 | R86 静态证据 PASS |
| s_handleSessionControl 入口定位 | PASS_STATIC @0x1594EC |
| s_handleSessionControl 参数/函数体语义 | NOT_YET_REVIEWED |
| iAPSendMessage 是否到达该 callback | NOT_PROVEN |
| inner_airplay_control 绑定真实 callback | NOT_BOUND |
| AirPlay event-channel single-reader pump | NOT_IMPLEMENTED_FOR_AU57x |
| QNX runtime / iPhone MFi / vehicle | NOT_TESTED |

接下来只做一项：对同一 SHA 的原厂 dio_manager 反汇编 0x1594EC 函数体，并将函数边界、ARM 指令、参数流、调用目标、字符串/CF API 交叉引用写入一个私有本地 audit；只公开不含 OEM 原始代码字节的结果摘要。若私有原厂 ELF 当前不在执行环境中，先从用户既有的交接资料恢复该文件，不重复索取已上传的同一文件。

不得把该静态 callback 入口当成可安装 hook。保持原厂 MFi 路径、有线 CarPlay与 fail-closed 策略。

installable=false / safe_to_flash=false / vehicle_verified=false / mfi_apple_verified=false。