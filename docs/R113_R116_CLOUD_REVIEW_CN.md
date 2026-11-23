# R113–R116 云端编译 / 原厂接口审计（源码审核分支）
2026-11-23。**NO INSTALL / safe_to_flash=false**。
本分支运行既有公开的 AU57x MU1005 34 个 C 源文件，确认 QNX 6.5 ARM 目标依赖和 QEMU smoke。
**明确范围：此工作流不包含用户本地 R108–R112 新增模块，也没有原厂 btstack 的真实 iAP2 注册/回调，因此即使云端全通过，也仅说明既有 native bridge 源码在交叉编译环境可链接或加载，不代表 R112 完整工程已经编译成功，更不代表无线 CarPlay 完成。**
R106 已有实际车辆日志仅支持 2.4GHz uap0 关联与 USB iAP2 / carplay0 / AirPlay 有线流。原厂有效 iAP2 SCN、BlueSDK owner、真实 MFi/Bluetooth iAP2 握手缺失证据。
不写车机文件、不取消有线优先、不绕过苹果认证、无安装脚本。
下一步：把 R112 的所有源码从本地交付包同步到此分支单独 CI，验证其目标编译；必须确认真实 OEM SDK 回调后才能考虑上车。
