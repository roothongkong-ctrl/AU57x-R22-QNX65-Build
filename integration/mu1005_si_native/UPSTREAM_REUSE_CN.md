# MU1005 原厂优先与上游源码复用（NO INSTALL）
固定上游 luka-dev/mib2q-carplay-rgi 20c94f51c0f0ae88b154c12e4f604643ba23022c。复用其原始 hook/wireless/mfi_stock.c，独立 ARM PIC 对象，不改认证逻辑。该对象只能在已合法绑定且加载真实原厂 libairplay 的同一进程使用。不能将 MFiPlatform 的符号链接成功认定为真实认证。禁止模拟证书、签名或成功返回。
离线原厂 ELF 核验：libairplay.so 目标哈希 193a4fd9101ec2aa05e7159cfa307b96500810d379ca74a194f172adc13a46b5；APSMFiPlatform_CopyCertificate 2 参数，APSMFiPlatform_CreateSignature 4 参数，原厂校验摘要长度 20 字节；原始 SAP 调用证书/签名函数后检查返回值。静态 ABI 与上游 wrapper 一致，不证明实际芯片执行成功。
原厂 Wi-Fi 分类函数存在且实际分类，USB 分类函数为常量 0；70版 DIO 配置转储含 MDNS_DIRECTLINK_IFACE=carplay0，三处 iap2.cfg 蓝牙段被注释。原车有网络/认证能力不等于原厂已有完整无线 CarPlay；不得仅改开关即宣称成功。
完整 hook 仍依赖上游未提交的私有 SDK 及适配 shim。congobg 的实车发布目标为 MU0917 等 MHI2Q，公开包为预编译组件；不能把 donor 二进制替换 MU1005 原厂程序。保留有线连接和恢复机制。
