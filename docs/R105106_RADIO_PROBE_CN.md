# R105.106 四路并行
R105.105 WiFi原厂分析成功，uaputl含双频标记，Marvell驱动存在；启动文本引用已定位，slogf在原厂已知依赖链中无可达导出，仍不视为链接通过。新增基于锁定QNX6.5 net80211 SIOCG80211/IEEE80211_IOC_CHANINFO的只读探测程序；只查询允许接口的频率及标志，不改变硬件、SSID、认证或配置，不以可查询信道推断AP能力。GCC/Clang 两路边界测试、QNX ARM编译、SDK libc.a和动态符号诊断四路独立。旧QNX flash首连仍在运行。NO INSTALL。
