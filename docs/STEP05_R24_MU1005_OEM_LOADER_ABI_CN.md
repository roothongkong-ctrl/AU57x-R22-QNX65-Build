# AU57x R24：第05～12步云端研发中期交付

基线：2017 奥迪 A6L C7，MHI2_CN_AU57x_K1006_2 / MU1005（原版 P0814）。
本包为只读研究审计、宿主机测试记录及验收台账；**不是实车安装包**。无原厂 ELF、QNX SDK、Apple 密钥、可写车机脚本。

## 固定进度

- 第01～04步：按照此前约定的验收范围已完成，其中第04步是原厂静态调用关系检查，不是无线连接成功。
- 第05～12步：允许云端继续开发，但当前所有步骤仍未达到**完整**验收。第13步必须保留实车安装验证。
- 用户已经提供过部分 K1006_2 原厂文件，在本轮从其 Library 重用，**不要求重新上传**。

## 本轮实际证据

1. 第05步：`bluetooth` 高层程序已知 iAP2 DSI 服务类型 `0x40000000`；这是服务掩码，**不是** RFCOMM channel。原厂 `mm-ipod` 动态加载模板 `ipod-transport-%s.so`、`dlopen`、`dlsym`、导出 `device_transport_init@0x103510`。原厂 USB 插件中 `ipod_module@0x3ec0`，模块 ABI v2 `0x200`，传输表 `0x40b8` 及 7 个函数指针得到**22/22** 只读验证。**不能**把 USB 插件 ABI 自动认作无线蓝牙插件；BlueSDK RFCOMM 到真实 iAP2 端点仍不明。
2. 第05步新增研究代码：GitHub R24 18 模块中的 `mu1005_rf_ingress.[ch]`、`mu1005_ipc_stream.[ch]`，规范化帧和发送代际，拒绝旧会话、边界超出和连接中断。属于应用级模拟收发代码，不是原厂蓝牙 Hook。
3. 第06步：从原有研发包复用的 MFi mock 生命周期**7项**宿主机测试通过（仅拒绝非法流程、清理等，不是认证成功）。
4. 第07～08步：原厂 `dio_manager` 和 `libairplay.so` 的 SHA 绑定审计，Wi-Fi/AirPlay 策略**11项**，原厂参照/篡改校验**3项**通过。实际手机关联、mDNS 广播和 AirPlay 会话未验证。
5. 第09步：仅有原厂静态 API 和 QNX ABI 证据，没有无线视频、音频、HID 会话。
6. 第10步：R23 既有有线优先、代际、断线清理状态机及主机测试；缺真实 SI/DIO 回调对接。
7. 第11步：R22/R23 的 QNX 6.5 编译、ELF 严格校验、QEMU 动态加载已有成功产物；R24 新增完整蓝牙规范化收发/IPC 回环测试，结果以 GitHub run 44930602924 的最终日志为准。
8. 第12步：原有备份/安装/独立恢复事务在主机上**67项**通过；缺此固件真实分区位置、全备份清单及恢复路径验证。现阶段**不得制作装车安装包**。

## 复验及复用

- 精确三份原厂文件：`bluetooth`、`mm-ipod`、`ipod-transport-usbdevice.so` 放在自己受控目录，不应推送公开仓库。
- `python3 tools/verify_step05_oem_loader.py --oem-dir /私有原厂目录 --output step05.json` 可做严格 SHA、ELF、7 个 USB 函数指针 22/22 检查。
- 自动构建：`https://github.com/roothongkong-ctrl/AU57x-R22-QNX65-Build/actions/runs/44930602924`。
- 前一版正式跑通：R23 `https://github.com/roothongkong-ctrl/AU57x-R22-QNX65-Build/actions/runs/44927713650`。
- 注意：调用关系和 ELF 一致，**不证明** Apple MFi 授权、真实蓝牙、同一设备身份、Wi-Fi 射频/5GHz、AirPlay 音视频、SI 控制权。