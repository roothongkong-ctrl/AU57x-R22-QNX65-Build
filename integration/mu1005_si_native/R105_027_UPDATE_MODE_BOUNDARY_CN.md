# R105.027 — 原厂模式同步与 iAP2 数据通道分离（NO INSTALL）

原厂 dio_manager SHA256 e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c，与 R105.026 相同。离线 ARM 反汇编，原始文件未修改；仅在分析副本修正一个越界动态符号 section index 供 llvm-objdump 读取。

- 0x1596c0 确认 tail branch 至 0x16dbe4。r0 来自 manager+0x228，r1=manager+0x2b4，r2=1。
- 0x16dbe4–0x16e12c 为候选指令范围（0x54c bytes），0x16e130–0x16e170 literal pool，0x16e174 后为独立代码。完整区间 0x590 bytes。
- 函数保存 r0/r2，r1 传至 0x16ab40，构造两个 shared pointer 输出；0x16dc8c 通过对象 vtable+0x3c 调用，r1/r2 为两个对象，r3 根据原 mode 选择 1 或 2；0x16dc90 后按返回值生成布尔结果。
- PIC 基址 0x1a6d48；literal 0x16e164 解析至 0x19c9f8 "updateMode"，0x16e168 至 0x19dcd0 的 CDSICarplayImpl HMI mode synchronization failure 日志，0x16e16c 至 0x19dc94 的 resources/appStates/valid 日志。
- 直接 PLT 调用由 .rel.plt 验证，主要为 shared pointer、UnicodeString、mutex、日志和错误处理；没有直接 CFLDictionary/CFData 取值、AirPlay send 或 iap2 raw send。内部 0x16ab40 及 0x16eae8/0x16eb50 尚未完整恢复。
- 结论：这一层是 HMI mode 同步路径，不作为 raw iAP2 数据入口；不能凭该静态证据证明整个下游不存在其他命令。真实 iAPSendMessage 分支及收发 owner 仍未确认。
- libairplay.so 的 AirPlayReceiverSessionSendCommand 和 AirPlayReceiverSessionControl 导出存在；不能将通用导出视作已绑定的无线会话。
- 继续复用 R103 LinkEngine、R86 delegate、inner_airplay 与 SI/DIO 现有源码；下一步只定位真实 command/data owner 以及原厂 BT producer。未提交 OEM 二进制，未修改 main，未重跑旧 CI，未新增运行时功能。NO INSTALL / NOT VEHICLE VERIFIED。
