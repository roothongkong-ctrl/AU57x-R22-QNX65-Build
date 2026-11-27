# R105.112 原厂运行优先路线
锁定LuKa main 20c94f51、joeyQuery main 3db007ae；前者11月24日主要更新控制台键，后者11月25日MH2p参考更新，无新的同版无线成功证据。复用原厂libiap2client.so.1、libstrm.so.1、libdns_sd.so.1，按R105.097确切SHA复制到私有QNX IFS，实际ARM dlopen(RTLD_NOW)和dlsym原厂7个iAP2与3个Bonjour接口，无替代库、无硬件/认证调用。仅日志发布。另将独立进程fixture的pipe换为已验证QNX AF_UNIX socketpair；生产CPRX、认证、fork、owner断言均不变。已通过wired-restore不重跑。原厂AirPlay完整加载仍缺NVIDIA库及硬件；不假造。NO INSTALL。
