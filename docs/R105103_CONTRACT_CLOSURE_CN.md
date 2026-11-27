# R105.103 四路无重复固件下载验证
R105.102 三路仍在运行，合成字节回滚测试已通过，实际配置验证未返回。新增四条独立云端任务，直接复用此前原厂报告与锁定 SDK 文件，不重编译、不解包完整固件。SDK lane 对原厂btstack未匹配的151个强导入进行实际去重符号覆盖，候选不等于原厂ABI。DIO lane 校验 native_exports.c 的真实 AirPlay 名称要求，video lane 校验屏幕导出与 NVIDIA 视频导入，MFi lane 校验原厂签名和证书接口存在，不执行或替代认证。四路均待测试，NO INSTALL。
