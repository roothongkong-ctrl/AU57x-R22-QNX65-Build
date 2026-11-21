# 13步固定验收台账

| 编号 | 验收项 | 当前结论 | 云端缺失/实车边界 |
|---|---|---|---|
| 01 | QNX6.5 ARMv7 原生工具链 | 已完成 | GCC build verified |
| 02 | MU1005 ABI / QNX QEMU loader | 已完成 | 与真实车机进程不同 |
| 03 | R23 SI/AirPlay基础代码 QNX测试 | 已完成 | 仅基础服务层 |
| 04 | 6份原厂ELF，47静态调用断言 | 已完成 | 非端到端无线链 |
| 05 | 蓝牙 RFCOMM ↔ iAP2 | **未验收**；R24规范化接口/原厂USB模块ABI静态审计 | 确切 BlueSDK 回调和原厂 iAP2 endpoint |
| 06 | MFi/SI 授权 | **未验收**；7项 mock 测试 | Apple 真实认证、SI动态授权事件 |
| 07 | Wi-Fi + 双频回退 | **未验收**；接口策略有测试 | 5GHz射频能力不明，Wi-Fi association未证实 |
| 08 | AirPlay/Bonjour | **未验收**；原厂接口匹配 + R23服务代码 | live mDNS、同设备身份、DIO监听 |
| 09 | DIO视频音频控制 | **未验收** | 媒体/HID真实流无证据 |
| 10 | 重连与有线优先 | **未验收**；局部状态机 | 真实 SI/DIO teardown/control callbacks |
| 11 | 云端联合回归 | **未验收**；QNX ELF/QEMU测试部分通过 | 缺实际 OEM 服务端边缘证据 |
| 12 | SD自动安装/备份/恢复 | **未验收**；67项事务主机测试 | 没有可验证原厂写入表，禁止实车安装 |
| 13 | 实车一次集中验证 | 未开始（最后一步） | 真实 iPhone、Wi-Fi/Bluetooth,有线保留 |

**不能把第05～12步的模拟/静态测试通过计为整步完成。**