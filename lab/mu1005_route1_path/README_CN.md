# 新第1项（原9）：配置路径只读确认
适用 MHI2_CN_AU57x_K1006_2 / MU1005。原始EFS中 etc/eso/production/connectivity.json SHA256 56c14940455215233f9ead5a8cd3e3a9dc984f372b0d7421ba26ff8e8f579470。既有车辆记录显示 /mnt/system 挂载，但没有配置实体及真实读取证据。候选主路径 /mnt/system/etc/eso/production/connectivity.json；候选别名 /etc/eso/production/connectivity.json。不能以模拟器内人为设置的别名证明实车路径。
本包复用 R105.155 已通过 ARM/QNX 测试的 config-path-probe，不重新编译。仅补充原厂 connectivity_launcher 的 FD 查询；不替换车机原厂文件、不挂载、不重启、不写配置，不采集密钥。停车并保持车机正常运行时，由现有SSH进入本包所在目录，校验 SHA256SUMS 后执行 sh probe.sh，保留终端输出。若SD执行权限受限，停止，不改系统权限。
验收：原始哈希匹配；两个路径是否同文件由 SAME_FILE/DISTINCT_FILE 判定，不能凭相同文件哈希认定别名。CONFIG_FD connectivity_launcher 为观察到的打开文件候选，瞬时读取可能错过；未出现不能判定不读取。若多个不同文件或哈希不符，不确定修改位置，停止。真实读取/生效仍按原9/11证据，不以工具执行成功替代。未验证实车前不可修改。
