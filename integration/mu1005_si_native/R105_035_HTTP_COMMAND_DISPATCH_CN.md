# R105.035 — MU1005 原厂 HTTP command 真实调用链（NO INSTALL）
原厂样本：用户既有 R9.zip/oem_reference/libairplay.so。此次实际取回原始文件并计算 SHA256：193a4fd9101ec2aa05e7159cfa307b96500810d379ca74a194f172adc13a46b5。dio_manager 同包 SHA256 e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c。llvm-objdump ARMv7 反汇编 + readelf .rel.plt，未发布原厂二进制。
- VA 0x2ca00 为 _requestProcessCommand：函数名来自 0x2cbac/0x2cbb0 解析 PIC 日志标签 0x962cc。实际指令至 0x2cbf8，literal pool 0x2cbfc–0x2cc14。
- 0x2e18c 真实调用该函数。request +0x209c 和 +0x20a8 传给 CFCreateWithPlistBytes (PLT 0x15d20) @0x2ca48，要求 dictionary；0x2ca84/0x2caa8 调用 CFDictionaryGetTypedValue (PLT 0x17c94) 获取 type (CFLString @0x95aa0) 与 params (CFLDictionary @0x95ab0)。
- 0x2cab0 从连接相关对象 +0xf4 获取 session。0x2cad8 经 PLT 0x18360 / GOT 0xab430 调用 AirPlayReceiverSessionControl，确认为六参数：r0=session, r1=1, r2=command, r3=NULL qualifier, [sp]=params, [sp+4]=response**（初始 NULL）。另一调用点 0x2ccc4 为不同 handler，不能全局吞掉。
- 0x2cadc 检查 OSStatus；非零走 0x2cbcc (422)。成功且响应为空，0x2cb4c 调 CFDictionaryCreateMutable 创建空字典；0x2cb00 调 0x2bde0 生成响应，后释放字典。解析异常 400，session 缺失 454，缺命令 451。这一上游调用点保留返回状态，区别于 0x25d94 后丢弃 delegate 返回值的路径。
- 新增 mu_inner_session_http_command：复用现有 split parser 和 socket relay；仅处理已绑定无线会话的 iAPSendMessage；无关命令交回原厂；接收成功才返回 OSStatus 0，忙/异常非零。原厂正常生成空响应。非零对应原厂 422，未证明手机会重试，不能称背压已解决。
- 未实际绑定原厂调用点、未验证 QNX 动态符号 interposition 是否可用、未确认队列/线程与 event-channel reply pump，未修改 OEM ELF。此提交仅静态分析及源码适配与 host fixture。禁止安装。
