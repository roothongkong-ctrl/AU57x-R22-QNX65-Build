# R105.040 — MU1005 原厂 iAP2 job producer 定位及原生 ABI 适配（NO INSTALL）
实际分析原厂 dio_manager：SHA256 e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c。临时反汇编副本仍仅修正 dynsym 非法 section index，原始文件未改。
## 1. 原厂 CDIOManager::iAP2Connect
VA 0x156c48。0x156e0c literal 解析到 0x19a820 "iAP2Connect"；0x156e08 -> 0x198e18 CDIOManager jobID 日志。
0x156c60 调 manager+0x328 的 0x1661a0，后者取 CSIService+0x248/+0x24c，均非空时按第二对象+0x1c 返回 EXLAP granted=1 或 denied=2，否则返回 0 并写错误。0x166274 literal -> 0x19bf44 "isExlapGranted"。因此无线场景缺 CSIService 时不能简单强行调用高层入口或伪造 grant。
0x156ca0 配置键索引10 -> 0x130bac；0x156cc8 UnicodeString8(char const*,Encoding,unsigned,bool,unsigned*)；0x156cdc r0=manager+0x130，r1=&native UnicodeString8，r2=(EXLAP result==1)；0x156ce0 BL 0x124990；r0/r1 64bit jobID。0x156cec 析构 native string。高层 boolean 返回值不是设备已连接证明。键10的真实配置名称/运行值仍需独立确认，不直接写死 donor 路径。
0x15bb08–0x15bb0c 构造 manager+0x130 内嵌代理，参数3。0x124a84 构造函数设置 vptr 为 CIAP2ServiceJobProxy vtable+8；GOT 0x1a7a64 R_ARM_GLOB_DAT -> vtable 0x1a43c0。
## 2. job producer 0x124990
0x1249a4 读取 proxy+0x10 的64bit ID；0x1249c8 调 0x1248e8（分配72字节），0x1248a4 使用 CIAP2ServiceJob_Connect vtable 0x1a4360+8（GOT 0x1a72e4）。0x1249e0 写 job+0x10 ID，0x1249e4 复制 proxy+8 到 job+8，0x1249e8 调 UnicodeString8::operator=(const &) 写 job+0x1c，0x1249f0 存 flag 到 job+0x44，0x1249f8 调 0x1365c4 提交；0x124a00–0x124a04 失败清零返回 ID。
0x1365e0 取 job+8，合法范围1..29；0x136670 tailcall 0x1362fc（真实队列入口候选）。未恢复队列完整线程合同。
## 3. 消费者到原厂驱动
CIAP2ServiceJob_Connect vtable 0x1a4360+0x10 指向 0x124478；0x124488 调 0x124ac0，由 CIAP2ServiceJobListener::s_listenerArray 0x1a8c50（GOT 0x1a767c）按 job+8 索引选择实际 service，0x1244b8 调 service secondary vtable 首槽。
CIpodAP2Service vtable 0x1a6698（GOT 0x1a7770），secondary address point 0x1a66c0 首槽 0x17a554（this-4 thunk）-> 0x17a55c onJob_connect -> 0x17a054 -> iap2_connect PLT 0x1127f0。构造 0x179e08 设置两个 vptr 为 vtable+8/+0x28，分配连接对象并保存 service+8。由此 job producer 与消费者可静态连通；实际 listener index 对应的 live service 仍需证明。
## 4. 源码
新增可调用的 native job ABI 适配，使用原厂 proxy、原厂 UnicodeString8 和原厂 EXLAP 决策。既有会话模块保留，不创建第二个 iAP2 driver。通过 caller 提供的真实 owner 证明才能提交；一次提交后等待原厂 teardown，jobID 不视为连接成功。host fixture 验证64bit ID、重复提交、失败与退役。未取得实际运行时 manager/path/owner，不执行 OEM call，不安装，不宣称无线已实现。
