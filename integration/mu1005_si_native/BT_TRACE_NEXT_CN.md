# MU1005 原厂蓝牙入口定位（NO INSTALL）
对照 2026-11-25 上游：
- congobg/MHI2Q_WiFi_CarPlay @223b42f90bf7c15fbbbef23259da36caa9bd4994：QC 3638/5092 的实车报告使用原厂蓝牙启动 trace、ACL 事件和动态库桥接；不是 AU57x 可直接使用的二进制。
- luka-dev/mib2q-carplay-rgi @20c94f51c0f0ae88b154c12e4f604643ba23022c：SI 是 DIO 单一生命周期所有者，iAP2 bootstrap 与会话内部 transport 分离。
- joeyQuery/MHI2-wirelessCarPlay @7d9f2297c9ed0a9d9f2ed9964a9421d9569ee501：TRACE-015 说明 MU0678 enableIap 属于 topology/reconnect policy，RFCOMM allocator 独立；MU1005 不能沿用地址。
- 已有 R100 原厂审计没有找到 MU0678 的命名 endpoint 字符串；并不排除动态构造。R99 现行策略保留 iap2.cfg 原样，不能把历史双开关候选当作实际功能。

## 一次离线处理
在原电脑版工程找原始 btstack，运行：
```sh
python3 tools/mu1005_bt_trace_entry.py --btstack /path/to/btstack --out mu1005-bt-trace.json
```
仅输出地址候选与固定哈希，无原厂代码字节、不执行 ELF。先定位 Device ID/ACL trace 引用；再反汇编候选所在函数、追踪其线程入口与 SDK 调用，核对原厂注册与撤销时序。ARM 直接引用工具不能处理所有 PIC/GOT/Thumb 路径；空结果不是不存在。禁止将候选当作已验证 hook。

优先复用现有本地原厂 btstack、bluetooth、iap、ipod-drvr-iap2.so、smartphone_integrator、dio_manager、libairplay.so、connectivity_launcher；相应蓝牙原厂配置和 launcher 配置只读核对。缺失时一次性从旧 app.img 提取；不将 OEM 文件提交公开 GitHub。无需再次上车采集。
现有 QNX 自动重连已通过模拟硬件测试，不再作为此阶段的主要任务。实际 owner、认证、媒体及安装均未验证。
