# R105.086 原厂启动 trace 接入候选与云端成果收录
R105.085 13 项 host/native 任务已全部通过，run 45920034388 / 45920034626，ARM 包 artifact 15163622524。实际 SDK 查找修复：6/11 同名候选、159/364 强引用匹配；缺 libicuuc.so.58、libcomm.so、libosal.so、libutil.so、libiplcommon.so。无原厂 ABI 或启动证明。QNX first-link run 45920034385 尚运行。
本轮复用原厂已校验的 trace 导入槽、ARM variadic tail、原始 trace latch、自身只读 procfs reader 与现有 GOT CAS：在 opt-in QNX ARM btstack preload 构造路径中尝试绑定实际 OEM trace。仅当精确 OEM 文件、已解析原函数、可写非执行 GOT、实际调用点和格式字符串一致时才替换导入槽；始终先绑定原函数，不写代码段，不在回调调用 SDK，不把日志当成功注册。若 lazy binding 或阈值过滤，当前不会捕获；不使用 mprotect 或伪造事件。新 ARM 编译待结果，真实 OEM 依赖及实际服务注册仍未完成。NAS 不发起额外下载。
