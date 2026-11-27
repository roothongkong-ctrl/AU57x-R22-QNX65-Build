# MU1005 三项简化方案（NO INSTALL）
适用 MHI2_CN_AU57x_K1006_2 / MU1005。只读检查包，不安装无线CarPlay。
## 新4：直接使用原厂路径
R105.185已验证原始固件10个文件身份；R105.186发现原厂启动恢复文件；R106实车记录已知/mnt/app、/mnt/system布局。优先使用原厂现有路径：/eso/bin/apps/btstack（物理候选/mnt/app/eso/bin/apps/btstack）、/eso/bin/apps/connectivity_launcher、/mnt/system/etc/eso/production/connectivity.json、/mnt/system/etc/eso/production/dio_manager.json。实际文件身份及运行路径以同一次实车只读结果为准。恢复脚本原厂/eso/bin/reset_bt.sh已在app70中确认；它是蓝牙故障重置，不是文件恢复工具。
## 新5：不自研整机备份
使用用户已有M.I.B. 3.8的Backup/Backup Plus功能，保持原版菜单与脚本。上游Mr-MIBonk/M.I.B._More-Incredible-Bash固定3a2526a386f54ec7f49bbc32984ef7edb1106d33，backup.sh调用apps/backup -a；backupplus_all.sh包括app/system等镜像备份；ImageRestore.sh调用ImageMOD -b，不代表所有备份均可用该命令恢复。完整备份和对应恢复入口应使用实际版本菜单，不直接执行未知restore命令，不上传EEPROM、FEC、证书、个人配置或整机镜像至公开仓库。验证备份可读、完整、实际文件身份与版本一致；复用原有backup_mapping.py/backup_verify.py，仅离线核验必要目标文件。受控恢复先有独立恢复入口、正确备份、稳定外部电源及用户同意。
## 新6：独立目录，最少改动
复用现有tools/mu1005_efs_launcher_stage.py和已验证原样参数转发，不再重做启动器。原厂launcher.btstack.exec、preCondition=/tmp/mvloaded、onFailure原厂reset_bt.sh均保留既有语义。LuKa固定20c94f51c0f0ae88b154c12e4f604643ba23022c的独立hooks与respawn设计仅参考；MU1316安装脚本不直接装入MU1005。计划独立目录/mnt/app/root/au57x-wireless（尚未证实可持久写入/执行），原厂二进制不替换；如该目录和启动入口不成立，停止而不是猜路径。已有native_binding及无线模块不重写，真正缺少的是原厂生命周期绑定。不能把路径存在、原厂ELF单独启动、夹具通过视为工厂launcher实际加载。
## 本包如何使用
本包只读，不改原车文件或配置，不启动、停止或重启服务。包含已通过R105.187的observe-identity、R105.173 config-path-probe及原有固定FD检查。SD复制到现有MIB的mod/au57x-readonly目录；custom_readonly.sh可由现有SSH在MMX上从此目录执行，或者由用户在确认无原有custom.sh冲突时通过MIB自定义脚本入口手工调用。切勿覆盖原有mod/custom.sh。先校验包SHA256SUMS；只在停车、车机正常时执行；输出固定路径、哈希匹配分类和文件身份，保存到SD，不记录完整进程命令行/密钥。若SD不可执行或检查失败，不改权限或挂载。实际结果须复核。独立目录是设计决策，不是已确认车机位置。新4/5/6均未100%。
