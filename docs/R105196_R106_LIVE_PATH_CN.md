# R105.196 使用用户实车日志（不再要求单独上车取路径）
依据用户本轮上传 001(7).zip，SHA256 cdb2617784d871aa561938ca11e83e346abfa54c0fa18c7a8cf48b9a1f817b63。58项含4目录、20快照。仅公开非个人路径/计数和技术结论，不提交日志原文、车机网络标识、设备身份等。
- metadata/oem_paths.txt 列出 /mnt/app/eso/bin/apps/{btstack,bluetooth,smartphone_integrator,dio_manager}、/mnt/app/eso/lib/{libairplay.so,libirc_mmx_adapter.so}、/mnt/app/armle/sbin/uaputl、/mnt/app/armle/usr/sbin/mm-ipod、/mnt/system/etc/eso/production/{smartphone_integrator.json,dio_manager.json}、/mnt/system/etc/mm/iap2.cfg。仅列出候选文件，不代表实际成功读取；OEM_BINARIES_SAVED=0。
- metadata/pidin_all.txt 实车参数为 /eso/bin/apps/btstack btstack 1、/eso/bin/apps/bluetooth bluetooth 4、./bin/apps/smartphone_integrator，以及 ./bin/apps/connectivity_launcher。mm-ipod实际 config=/etc/mm/iap2.cfg，认证路径 /dev/i2c8。20份快照均有前两个；DIO与mm-ipod在第3个有线快照首次出现（无线尝试6份均未见）。不能认定无线服务已注册或认证。
- metadata/mounts.txt: /mnt/system flash，/mnt/app qnx6；没有实际读写权限或独立恢复证明。connectivity.json未出现在上传日志；结合既有R105.109/119原厂固件，候选 /mnt/system/etc/eso/production/connectivity.json，/etc/eso/production/connectivity.json 为待校验别名，不硬称已生效。
- btstack_memory_map.txt 记录一次libcomm/libutil映射，支持实际原厂库加载，不将单次地址硬编码；现有动态GOT解析保持不变。
- 真正修复：旧trial.sh使用pidin -p name mem的截断名称且扫描所有字段，查询失败可能被当作没有有线会话。改为一次pidin ar，按真实PID/Arguments格式只看命令名，忽略grep/其他参数文本，失败/不明格式拒绝，保留原始恢复及校验。测试仅新的真实格式和失败情形，旧五路固件副本回归保留为手动。
- 原厂配置只启用已验证的bluetooth.enableIap，iap2.cfg原样；已通过的原厂生成器及恢复不重写。路径别名、实际挂载可写性、MIB独立恢复以及原厂启动消费须最终同次车机检查；不增加独立上车次数。当前NO INSTALL，仍缺原厂真实启动/生命周期和无线连接。
