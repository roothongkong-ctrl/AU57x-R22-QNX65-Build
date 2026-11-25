# 原厂动态符号依赖实测（禁止安装）

2026-11-25：从既有 MU1005 资料提取 Root IFS Part-2，680 个 LZO1X 块、44,513,900 字节、139 个普通文件，imagefs 声明长度 44,513,640 字节且 32 位累加校验和为零；未发现此前缺失库。

使用 GNU readelf -D -sW（支持 IFS 去节表 ELF），对真实原厂文件动态导入及导出集合进行静态覆盖分析，默认版本 @@ 同时可匹配 @指定版本和无版本名称。SI 351/351 强导入覆盖；32 种同名库选择中 12 种全递归强导入集合覆盖。最优集合含 Root IFS 的 libosal.so 和 stage2 的 libcomm、libutil、libiplcommon、libdsicommon、libservmngt，以及此前补件 libirc_mmx_adapter。不能据此认定混用库 ABI 兼容。全 stage2 的候选存在一个未覆盖名称。

btstack 360/364，递归缺 5 个名称：xmlCreatePushParserCtxt、xmlCtxtUseOptions、xmlFreeParserCtxt、xmlParseChunk、icudt58_dat。缺少 libxml2.so.1、libicudata.so.58。DIO 375/376，递归缺 196；AirPlay 212/226，递归缺 193。DIO 直接未覆盖名称为 ppsparse。无需以模拟库伪装这些依赖。

原厂二进制仍在用户私有资料中，未上传 GitHub。符号集合覆盖不证明链接器版本绑定、重定位、初始化、进程间服务、硬件、MFi 或实际无线连接。原厂 SI 独立 QNX 启动尚未执行；当前容器无 QEMU/Docker。云端 45541185606 仍在测试此前自研库的 IFS 原始字节加载，不是 OEM 运行。下一目标是在授权私有 QNX 环境中使用真实原厂 SI 及已恢复依赖，验证加载和初始化，不替换车机文件。