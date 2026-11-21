# R41：原厂 MFi + iAP2 CONTROL 认证阶段源码接入（NO INSTALL）
目标：MHI2_CN_AU57x_K1006_2 / MU1005，2017 Audi A6L C7。

**离线样本实证**：用户已有 R14 原厂 libairplay.so SHA256 193a4fd9101ec2aa05e7159cfa307b96500810d379ca74a194f172adc13a46b5，ELF ARM32 的 APSMFiPlatform_CopyCertificate @0x6d1d0 与 APSMFiPlatform_CreateSignature @0x6d4a8 均确实导出；签名函数反汇编在 0x6d4fc 比较挑战长度20。未将原厂 ELF 上传公共 GitHub。

**本次真实代码**：cp_step04_stock_mfi.[ch] 接收真正 OEM 所有者已经加载且匹配上述样本的 libairplay handle，通过 dlsym 调用原厂证书/20字节签名 API。必须有 owner 线程、原厂镜像、同代际 iAP2 CONTROL session 和无有线占用的证据；函数调用不等同 MFi 成功。不自行打开 I2C、不重复 dlopen、不实现假签名。
cp_step04_iap2_auth.[ch] 实现控制会话10的 AA00→AA01、AA02→AA03、AA04失败、AA05手机成功闭环；将排队TX和实际TX确认分开；重复和畸形消息拒绝，成功后必须每次重新验证 OEM 会话与手机接受，证据丢失后永久禁止旧会话复活；cp_step04_dispatch 将认证证据安全地接到无线控制面。
测试文件中的 APSMFiPlatform_* 为合成主机桩，不能用于真实车机。QNX研究库仅编译及加载符号检查。

**仍缺实际原车闭环**：Bluetooth BlueSDK RFCOMM 原始所有者 → OEM mm-ipod / ipod-drvr-iap2.so 控制端点是否可接，iAP2 link重组/传输生命周期，iPhone AA05 真实认证成功回调、原厂 libairplay 句柄/线程/分配内存证明，均未得到实车验证。静态 ELF 和主机桩不能证明这些条件。固定第04步仍然 PARTIAL / NO_GO。
