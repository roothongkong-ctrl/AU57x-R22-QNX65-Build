# R105.013｜第 03 步：mm-ipod 插件加载机制与原厂 USB Transport ABI

日期：2026-11-26
路线：R105 固定 22 步
目标：2017 Audi A6L C7 / MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
状态：MM_IPOD_LOADER_STATIC_PASS / USB_PLUGIN_ABI_STATIC_PASS / BT_PLUGIN_NOT_FOUND_IN_ARCHIVE / NO_INSTALL

## 1. 本轮复用的原厂文件

从用户此前的完整私有交接包直接提取，没有要求重新上传：

| 文件 | 大小 | SHA-256 |
|---|---:|---|
| `mm-ipod` | 40,297 B | `4b87cd2d41ddcee2f38800feafe8b4b340e4f23e1d86c7e97af303522aa15d09` |
| `ipod-transport-usbdevice.so` | 19,338 B | `e1743821b166fcd93992b45fc7064ad14763ee8a4445478e9aa36e696f2f6e95` |
| `ipod-transport-usbhost.so` | 20,912 B | `be3d8f4076d03408624415b8e324ca70ccc437dc7d0894f0eadbd356e5b410a0` |
| `bluetooth` | 1,410,944 B | `7d5ae31396f016f811e18a7255bf1311e0bfa471b2a3252a6f2eb4bd3ad04272` |
| `btstack` | 1,846,436 B | `51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566` |
| `libasimmxconnectivity_bluetooth_iapproxy.so` | 48,248 B | `44883ed28e44a8f06b5308266510981e17b3ee59d737f73208cd76e11ffc1c80` |

均为 ELF32 ARM；其中 mm-ipod 和两个 USB transport 是 QNX 原厂模块。

## 2. 关键突破：mm-ipod 不是把 USB 写死，而是动态加载模块

在 `mm-ipod` 中静态定位到：

| 函数 | VA | 长度 |
|---|---:|---:|
| `iface_find` | `0x1033a4` | 364 B |
| `device_transport_init` | `0x103510` | 228 B |
| `device_drvr_init` | `0x1035f4` | 200 B |
| `ipod_resmgr_mount` | `0x103ad0` | 512 B |
| `ipod_main` | `0x103d8c` | 1,320 B |

`device_transport_init` 按配置中的 transport 名称构造动态库名：

```
ipod-transport-%s.so
```

随后查找 `ipod_transport` 接口。通用 `iface_find` 路径：
1. 对动态库导出的 `ipod_module` 执行 `dlsym`；
2. 校验模块 ABI major（descriptor +0x04 的高字节为 2）；
3. 取 descriptor +0x20 指向的 transport context；
4. 检查该 context 的接口名称；
5. 将 mm-ipod owner/context 关联到插件；
6. `device_transport_init` 调用 context +0x14 的初始化函数，并传入设备挂载路径与模块参数。

驱动模块使用同一类动态模块命名方式：`ipod-drvr-%s.so`。因此，mm-ipod 的外层 transport 确实是插件化架构，而不是只能使用固定 USB 实现。

## 3. 原厂 USB Device 插件给出了可复核的 ABI 模板

`ipod-transport-usbdevice.so`：
- 导出 `ipod_module`：VA `0x3ec0`，36 bytes；
- 内部 transport context `transport_usbdevice`：VA `0x40b8`，168 bytes；
- 模块 descriptor 指向该 context；
- context 的回调槽位如下：

| context offset | 函数 | VA |
|---|---|---:|
| +0x14 | init | 0x1934 |
| +0x18 | get_link_params | 0x1874 |
| +0x1c | type_get | 0x18bc |
| +0x20 | caps_get | 0x18cc |
| +0x24 | audiopath_get | 0x1910 |
| +0x28 | data_send | 0x1560 |
| +0x2c | data_recv | 0x1228 |

真实底层实现可从 ELF 直接复核：
- send 路径调用 QNX USB bulk/interrupt endpoint API；
- receive 路径经 USB URB 回调进入循环缓冲区，再由 data_recv 交给 iAP2 driver；
- link 参数、transport type、能力和音频路径分别有独立回调。

## 4. 第二份 donor 交叉核验：USB Host

`ipod-transport-usbhost.so` 也导出 36-byte `ipod_module`，context `transport_usbhost` 为 180 bytes。其 init、link params、send、receive 槽位仍为 +0x14、+0x18、+0x28、+0x2c，与 USB Device 的共同 ABI 前缀一致。

这说明前述七槽结构不是单个 USB Device 插件的偶然布局，而是至少被两份原厂 transport donor 共同使用的接口形状。

## 5. 蓝牙侧目前确认到哪里

原厂 `bluetooth` 主程序包含：
- `SERVICETYPE_IAP2`；
- RFCOMM channel 错误处理字符串；
- BluetoothSmartphoneIntegration 相关符号。

`libasimmxconnectivity_bluetooth_iapproxy.so` 包含 `asi.connectivity.bluetooth.iap.IapDeviceServices` 和 RPC Stub/Proxy。它证明原厂 Bluetooth 应用有 iAP 服务管理接口，但它是 ASI RPC 代理证据，不等于 mm-ipod 的原始字节 transport 插件。

## 6. 本次资料包的模块清点结果

在这份私有交接包中找到的 `ipod-transport-*.so` 只有：
- `ipod-transport-usbdevice.so`
- `ipod-transport-usbhost.so`

没有找到：
- `ipod-transport-bluetooth.so`
- `ipod-transport-wifi.so`

注意：这是**本次检查的资料包清单结论**，不代表已经穷尽 K1006_2 的全部 app.img、stage2、其他分区或实车文件系统。不能因此宣称原车固件绝对不存在蓝牙插件。

## 7. 本步结论

已完成：
1. mm-ipod 动态 transport 插件加载路径；
2. `ipod_module` descriptor 查找和 ABI major 校验；
3. transport context 初始化入口；
4. USB Device 七个核心回调槽位；
5. USB Host 对相同核心 ABI 前缀的交叉验证；
6. 原厂 Bluetooth iAP 服务管理证据与 mm-ipod transport 层的边界区分。

尚未完成：
- 找到本车实际 BluetoothTransportComponent 的 constructor/vtable；
- 找到蓝牙 RFCOMM 原始字节回调如何进入 `ipod-transport` ABI；
- 找到 Wi-Fi 物理 transport 插件；
- 证明 iPhone 无线 CarPlay 建链、MFi 认证和媒体流；
- QNX 真机运行与实车验证。

## 8. 下一步建议：第 04 步

优先对照原厂 `bluetooth`、`btstack`、`libirc_mmx_adapter.so` 与 `ipod-drvr-iap2.so`：
- 追 RFCOMM server 注册、服务 UUID/channel 与回调 owner；
- 找出原始字节收发是否可由 BlueSDK/原厂 ASI 组件提供；
- 证明它能否以非阻塞方式接入 mm-ipod 的 +0x28/+0x2c 插件 ABI；
- 如果 stock transport donor 确实不存在，再评估新增 transport plugin 的最小接口与安全边界。

验收状态：
- mm-ipod dynamic loader：PASS_STATIC
- USB Device plugin ABI：PASS_STATIC
- USB Host shared ABI prefix：PASS_STATIC
- Bluetooth iAP service management：PASS_STATIC
- Bluetooth raw byte transport：NOT_PROVEN
- Wi-Fi transport: NOT_FOUND_IN_INSPECTED_ARCHIVE
- vehicle runtime：NOT_TESTED
- installable：false
