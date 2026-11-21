# R31：MU1005 原厂 mm-ipod 传输加载入口审计

原厂 `mm-ipod` ELF（40,297字节）完整 SHA-256 为 `4b87cd2d41ddcee2f38800feafe8b4b340e4f23e1d86c7e97af303522aa15d09`，与 R24 USB 插件 ABI 审计记录一致。对已保存的同版文件做只读校验：

- `device_transport_init`、`dlopen`、`dlsym` 导入/导出标记存在。
- 动态插件模板 `ipod-transport-%s.so` 存在，说明传输实现依赖参数选择和加载模块。
- `/dev/ipod`、`ipod-drvr-%s.so`、帮助文本 `usbdevice,usbhost` 存在。它们不能证明运行时选定哪个 transport；也不能仅凭帮助文本断言不支持蓝牙。
- 原厂高层 `bluetooth` 中有 iAP2 DSI 相关字符串，但目前仍未找到 Bluetooth RFCOMM 与该 `mm-ipod` 资源管理器之间的**运行时可证明桥梁**。

检测器：`tools/verify_mu1005_ipod_loader.py`。原厂样本仅在本地使用，公开仓库保存指纹和静态报告，不上传 ELF。此证据只补全固定第02步的**静态**原厂入口；动态认证/无线会话仍未验收，不能制作装车刷写包。
