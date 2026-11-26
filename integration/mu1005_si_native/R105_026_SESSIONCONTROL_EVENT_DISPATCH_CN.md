# R105.026｜MU1005 SessionControl 事件对象与主线程分发复核

日期：2026-11-26  
样本：R9.zip 内 `oem_reference/dio_manager`  
SHA-256：`e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c`（690843 bytes，匹配原厂审计）  
方法：提取原厂 ELF .text 后按 ARMv7 little-endian 静态反汇编；未执行 QNX 程序。

## 1. 上一轮遗漏的关键证据已补齐

R9.zip 确实包含完整原厂 `dio_manager`，不是只有 audit JSON。文件字节数和 SHA-256 均精确匹配 R86/R10 审计记录。本轮因此直接恢复了下游真实指令，不再依赖函数名猜测。

## 2. 真实的 callback → event → receiver 路径

### A. AirPlay callback：`s_handleSessionControl`

- VA：`0x001594EC`，164 bytes
- 入口 ABI：r0=session、r1=CFLString*、r2=const void*、r3=CFLDictionary*；入口栈参数含 CFLDictionary** 与 context。
- 它将 command string 与 dictionary 送入内部 event construction helper（VA `0x12C2E0`），随后调用 `s_waitAudioAvailableAckEvent`（VA `0x158DC4`），等待结果。
- 该 callback 自身没有直接执行 dictionary key 解码。

### B. Event receiver：`onEvent_sessionControlHandle` 候选实现

- VA：`0x00159598`，位于 `CDIOManager` vtable（VA `0x001A58D0`）对应的成员函数表内。
- 它把 event 对象的 6 个 32-bit 字段（offset 0、4、8、12、16、20）复制到 manager 的内部暂存区（manager offsets `0x2B4`、`0x2B8`、`0x2C0`、`0x2C4`、`0x2BC`、`0x2C8`）。
- 它随后 tail-call VA `0x16DBE4`，并将 manager 内部 context、暂存区地址和 mode=1 传入。
- 从行为看，这是事件接收/暂存/转交层；目前不能仅凭 vtable slot 将它绝对命名为 onEvent 方法，但其附近只读字符串表存在 `onEvent_sessionControlHandle`，与此路径一致。

### C. Event construction / object layout

- VA `0x12C2E0` 将输入对象构造成内部 event，保存 string/dictionary/context 相关字段。
- 同一事件类 vtable：`CAirPlayServiceEvent_SessionControlHandle`，VA `0x001A48E8`，大小 36 bytes。
- callback 等待 helper 的返回值最终从 context 偏移 `+0x238` 读取。

## 3. 仍不能安全宣称的内容

- 事件对象 6 个字段的完整语义尚未全部确定；只能确认其复制偏移，不能把字段顺序直接写成 iAP2 参数结构。
- VA `0x16DBE4` 的完整语义和真实命令分支尚未完成恢复。
- 字符串 `OSStatus dio::CDIOManager::handleSessionControl(...)` 与 `handleSessionControl` 确实存在于原厂字符串区，但尚未将该名字唯一绑定到一个完整函数范围。
- 未证实该事件允许携带 iAP2 raw bytes；也未证实它可用作无线 iAP2 数据面。

## 4. 下一步精确目标

1. 完成 VA `0x16DBE4` 的函数边界、参数流和所有关键调用分析。
2. 追踪 `0x16DBE4` 内部对 session、CFLString、CFLDictionary、返回 dictionary 的读写。
3. 恢复 6 字段 event struct 的类型、所有权和响应路径。
4. 对照 `libairplay.so` 的实际导出/导入 ABI，确认是否存在原生 iAP2 传输回调；不能把控制事件当作传输接口。
5. 在命令 schema、对象生命周期和线程语义全部确定前，保持无线 raw bridge 关闭。

## 5. 本轮结论

- 原厂二进制已重新提取并校验 SHA-256：PASS。
- callback → event constructor → wait path：静态确认。
- receiver 暂存字段 offsets：静态确认。
- onEvent 精确命名：高置信推断，待唯一函数归属确认。
- handleSessionControl 完整函数体及 iAP2 schema：未完成。
- 实车、MFi、AirPlay 连接、Wi-Fi、安装：未验证。

`research_only=true`, `safe_to_flash=false`, `vehicle_verified=false`.
