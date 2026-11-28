# MU1005 一次首连前检查（R105.245，非安装包）
R105.244 云端已完成打包。历史车机 R106/R105.196 真实日志没有 connectivity.json 的实际生效路径，只有候选路径；不能用编译或固件镜像冒充实车证据。修正旧整合脚本：即使候选文件缺失也记录两条路径、固定文件状态及原厂进程；实际调用已编译的只读观察工具。默认不写原车配置、不重启、不安装。
停车，保持手机已经蓝牙配对/连接，拔掉USB。解压 tar.gz，将全部目录复制到SD卡第一槽的独立目录（例如 /fs/sda0/mu1005-first-link）。用已有SSH进入MMX，执行 cd /fs/sda0/mu1005-first-link && sh first_link_session.sh observe 。交回 first-link-result.txt 和存在时 observer/bt-live-result.txt。无需重复采集既有固件或车机日志。工具仅写SD卡报告；若报告已存在，保留旧报告并停止。此报告不能证明无线CarPlay或配置生效，不能授权安装。apply 仍禁用，尚缺原厂实际配置加载和恢复路径证据。
