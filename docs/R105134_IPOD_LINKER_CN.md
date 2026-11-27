# R105.134 原厂 iAP2 加载原因定位（NO INSTALL）
R105.132 run 45975148003：原厂 USB transport dlopen OK，iAP2 driver dlopen BLOCKED，静态依赖 missing0/ambiguous0，mm-ipod exit0，未观察到 /dev/ipod0，其他 transport 候选0。R105.133 run 45976294961 软件源环节通过，保留其正在执行的任务。
修正旧启动器丢弃 dlerror 的缺陷：DRIVER_NOW、USB_NOW 分别记录真实加载结果；仅在 driver NOW 失败后做 LAZY 对照。不调用模块函数、不模拟认证、不把 LAZY 成功当作服务就绪。私有日志仅输出分类和允许的库/符号名称，增加分类器自测；复用已保留固件和原始插件，不重复启动12路实验。新运行尚待验收。
