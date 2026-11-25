# R98 原厂蓝牙 owner 移交修正（NO INSTALL）
首轮统一云端 8/9 成功，graft 三个 ARM 对象已编译但缺 ld.lld；恢复原有安装步骤。R98 connect 最长等待1秒，原来只在连接前检查 owner 与传输资格，现增加连接完成后的独立重新确认；期间撤销则关闭 socket、不进入 native attach。poll 避免 FD_SET 对超过 FD_SETSIZE 的描述符越界。新增真实 loopback 负例，原有一次性 native 绑定/清理语义保留。新 ARM 任务编译真实 R98 owner 源文件为独立 PIC 对象，不表示完整插件已链接或原厂 owner 已提供。
尚未完成：真实 btstack iAP2 owner、daemon 端独立身份认证、mm-ipod transport 插件的完整运行、会话认证/清理与 Wi-Fi 首连。TCP localhost 本身不是对端身份认证。无 MFi 绕过，不安装，不改 main。
