# R105.159 第7项：原厂启动参数确认

只做固定66项中的第7项。复用现有 tools/mu1005_efs_launcher_stage.py 的 WRAPPER_TEXT（与 omonob 固定提交 aec37f5b 的保留原厂程序、exec "$@" 结构一致），不复制外部接入库、不改车机、不重跑R105.158静态扫描。R106 pidin 实车显示 btstack 1、bluetooth 4；不是原始 argv[0] 的逐字证据，原厂实际配置 reader/启动管理器接入属于后续第8–10/17项。

验收分层：原厂精确配置副本只改 exec 且原参数字段不变；主机真实 exec 验证原样 argv[1:]、PID、退出码、关闭/缺库回退、合法测试预加载和既有环境；QNX guest用SDK shell和ARM无害目标实际执行并校验参数、PID、退出码。不伪造真实蓝牙或硬件。若无真实配置显式参数字段，报告其缺失，不猜来源；第7项云端参数转发合同通过后仍须注明真实OEM supervisor运行未证，不以夹具证明原厂启动成功。NO INSTALL。

计划12个独立host场景+1个原厂配置+1个QNX guest=14路，另自动通用回归4路，总计划18路。已检查近期Actions均结束。CI结果待核实。
