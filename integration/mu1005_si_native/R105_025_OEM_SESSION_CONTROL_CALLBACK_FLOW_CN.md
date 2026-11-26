# R105.025｜MU1005 原厂 s_handleSessionControl 真实函数体与事件转发链

日期：2026-11-26
适配基线：MHI2_CN_AU57x_K1006_2 / MU1005 / QNX6.5 / ARM32 little-endian
原厂输入：`/mnt/data/oem_private/dio_manager`，SHA256 `e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c`；与 R86 pinned OEM 文件完全一致。
性质：只读静态反汇编。未修改原厂文件、未注入进程、未安装、未上车。

## 1. 函数边界与真实 C++ 签名

ELF 动态符号表给出的函数为：

`dio::CDIOManager::s_handleSessionControl(AirPlayReceiverSessionPrivate*, CFLString*, void const*, CFLDictionary*, CFLDictionary**, void*)`

- VA `0x001594EC`
- ELF 符号大小 164 bytes，结束于 `0x00159590`
- 回调注册槽：R86 已证实 session delegate offset +16 指向该函数（GOT slot `0x001A7A3C`）。

因此这是一个真实原厂 session callback，不是我们新写的 hook。AAPCS 参数位置按该签名与入口指令可恢复为：

| 参数 | ABI 位置 | 类型 |
|---|---|---|
| 1 | r0 | AirPlayReceiverSessionPrivate* |
| 2 | r1 | CFLString* |
| 3 | r2 | void const* |
| 4 | r3 | CFLDictionary* |
| 5 | [entry SP] | CFLDictionary** |
| 6 | [entry SP+4] | void* |

入口 prologue 建立 64 字节栈帧后，从 `[sp+0x44]` 取出第 6 个参数；原始 r1 与 r3 分别保存在 r6、r7。此参数映射与 demangled symbol 签名一致。

## 2. 函数体实际做了什么

从 `0x1594EC` 到 `0x159584` 的 164 字节函数体可见：

1. 建立栈帧并保存 session callback 参数。
2. 进入 OSAL 同步/锁辅助路径（调用内部 helper `0x12C5B8`）。
3. 检查第 6 个参数（void* context）是否为空；为空时使用负错误状态，不执行等待成功路径。
4. context 非空时，把第 2 个参数 CFLString* 与第 4 个参数 CFLDictionary* 传入内部事件对象构造/封装 helper `0x12C2E0`。函数本体没有直接调用 `CFDictionaryGetCString`、`CFDictionaryGetInt64` 或 `CFDataGetBytePtr` 等字段解码 API。
5. 随后调用原厂 `s_waitAudioAvailableAckEvent(int, void*) @0x158DC4`，传入第 6 个 context 与 `-1` 等待参数。
6. 等待成功时从 context 对象偏移 `+0x238` 取出结果指针；失败时生成负错误状态。
7. 退出同步/锁辅助路径，返回结果指针/错误值。

这说明 `s_handleSessionControl` 是 AirPlay session control callback 的同步转发/等待入口。它把 CFLString 与 CFLDictionary 交给原厂内部事件路径，并等待另一个执行者完成处理；不能把它等同于 iAP2 raw-message parser，也不能在这里直接假定存在 `{type:iAPSendMessage, params.data}` 的明文对象访问。

## 3. 下游原厂事件路径的独立静态证据

同一原厂 ELF 的字符串与 vtable 中存在以下配套符号名/类型名：

- `OSStatus dio::CDIOManager::handleSessionControl(AirPlayReceiverSessionPrivate*, CFLString*, const void*, CFLDictionary*, CFLDictionary**)`
- `onEvent_sessionControlHandle`
- `CAirPlayServiceEvent_SessionControlHandle` vtable（VA `0x001A48E8`，动态符号大小 36 bytes）
- `handleSessionControl`
- `waitAudioAvailableAckEvent`

这与 callback 的事件转发和等待结构相吻合。vtable 指向的事件分发实现中可见 ActiveObject listener 分发：取 listener、取 event，按 listener vtable 中的回调槽间接调用。该处证明了存在 typed event/listener 模式，但仅凭字符串和 vtable 名称，还不能断言具体 event handler 的所有字段偏移或每个调用顺序。

## 4. 对无线 CarPlay 接入设计的影响

当前 inner_airplay_control 的入口接收已解码 dictionary 并提取 `iAPSendMessage`，但原厂 callback 的当前静态证据只证明它把 CFLString 与 CFLDictionary 交给内部事件路径并等待响应。下一步应优先反汇编并恢复 `handleSessionControl` 的下游实现及 `onEvent_sessionControlHandle` 的实际调用链，重点确认：

1. `CFLString*` 表示的 command/method 名称；
2. `CFLDictionary*` 中实际可见的 key/value schema；
3. `CFLDictionary**` 输出指针如何写入响应对象；
4. 第 6 个 context 的 owner、生命周期与 `+0x238` 字段类型；
5. handler 是否只处理标准 AirPlay control 命令，是否存在可扩展的 iAPSendMessage 命令；
6. 事件处理和同步等待是否允许桥接异步 iAP2 消息，超时/取消如何返回。

**严禁**在未恢复上述 schema 和生命周期前，直接在 callback 内解析未知字典、保存裸指针、跨线程持有 CF 对象，或绕过原厂 handler。桥接必须遵循单 owner、引用计数/复制语义和原厂返回值。

## 5. 本轮结论

- R86 delegate owner/slot：已复用，不重复研究。
- s_handleSessionControl 函数边界、签名、参数位置：静态确认。
- callback 会将 CFLString 与 CFLDictionary 交给内部事件 helper，并等待 `s_waitAudioAvailableAckEvent`：静态确认。
- callback 自身是否直接读取 `iAPSendMessage` 字段：未见直接字段读取，不能声称已识别。
- 下游 `handleSessionControl`/`onEvent_sessionControlHandle` 的完整函数边界、参数流、命令 schema：仍待下一项逆向。
- 生产注入、AirPlay 真正连接、MFi、Wi-Fi 媒体流、实车：未验证。

installable=false / safe_to_flash=false / vehicle_verified=false / mfi_apple_verified=false。