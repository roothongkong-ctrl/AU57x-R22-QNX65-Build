# R105｜001 实车快照与原厂 Transport producer 交叉核验

日期：2026-11-26  
输入：用户上传 `001(2)(1).zip`（约 122 KB）及此前精确匹配的 `ipod-drvr-iap2.so`。  
状态：PARTIAL / NO_INSTALL。

## 1. 001 快照直接支持的事实

### 1.1 实车进程清单

`001/metadata/pidin_all.txt` 中记录：

- PID 1208409：`/eso/bin/apps/btstack btstack 1`
- PID 1265702：`/eso/bin/apps/bluetooth bluetooth 4`
- PID 67620983：`mm-ipod -d iap2,config=/etc/mm/iap2.cfg,probe,ppsdir=/ramdisk/pps/iap2 -a i2c,addr=0x11,path=/dev/i2c8,speed=50000 -t usbdevice,path=/dev/io-usb/otg_dev -l /dev/shmem/iap2.log -vvvvvv`
- 同一快照还记录 `dio_manager` 与 USB NCM device driver。

这证明采集时原厂 iAP2 的 mm-ipod 实例明确使用 `-t usbdevice`，蓝牙栈和 bluetooth 服务作为独立进程同时存在。**它不证明蓝牙 iAP2 transport 已注册，也不证明蓝牙 RFCOMM 与 iAP2 驱动相连。**

### 1.2 线程快照

`001/metadata/btstack_threads.txt` 显示 btstack 内有：

- `Thread_bluetoothProc`
- `luesdk_mv8787bt_read`
- `uesdk_mv8787bt_write`
- `Thread_telephoneProc`
- `AgentThread_media`

这些名称说明 btstack 进程包含 BlueSDK/蓝牙读写相关线程；但线程名本身不能给出 RFCOMM SCN、socket FD、回调 ABI 或可供 iAP2 直接调用的 API。

### 1.3 内存映射与结果

- `001/metadata/btstack_memory_map.txt` 和 `bluetooth_memory_map.txt` 主要是进程映射/对象记录，没有提供可反汇编的 ELF 本体。
- `001/RESULT.txt` 明确记录 `IAP2_OEM_OWNER_VERIFIED=no (requires native evidence)`。
- ZIP 的 `001/oem_files/` 目录为空，没有 `mm-ipod`、`ipod-transport-usbdevice.so` 或 Bluetooth transport ELF 可供反汇编。

## 2. 对 R104 推断的约束

R104 从 `ipod-drvr-iap2.so` 机器码已确认 iAP2 协议引擎消费上层提供的 transport object，vtable slots：get_link_params +0x18、send +0x28、receive +0x2c。

结合 001 实车快照，目前只能得出：

1. 原厂当前实车启动路径确实显式选择 `usbdevice` 物理 transport。
2. btstack/bluetooth 运行于独立进程；不能假设 iAP2 driver 直接调用这些进程内部函数。
3. 若要实现无线 iAP2，需有一个可证明的进程间/插件边界，把 RFCOMM 数据可靠送入 iAP2 transport object。
4. 001 没有捕获 iAP2 Bluetooth transport 实例或 Bluetooth RFCOMM attach；结果文件也明确标记 owner 未验证。

## 3. 本轮新增的明确结论

**目前没有证据证明 MU1005 原厂 mm-ipod 已加载 Bluetooth transport 插件。** 相反，001 的实车命令行明确显示本次实例以 `-t usbdevice` 启动。是否还有另一实例/配置路径在其它启动条件下启用 Bluetooth transport，当前数据不能回答。

不能据此把 `-t usbdevice` 改成猜测的 `-t bluetooth`，也不能直接将 btstack 线程回调写入 R104 vtable。

## 4. 下一步（优先利用现有资料）

1. 从用户原厂 app.img / 固件中提取同版本 `mm-ipod`、`ipod-transport-usbdevice.so`、完整 `/etc/mm/iap2.cfg`，对照 R104 ABI 追踪 loader 与 plugin ABI。
2. 在现有 001 证据中检索是否存在第二个 mm-ipod 实例、RFCOMM channel/SDP 记录、btstack IPC endpoint；若没有，明确记为缺证，不继续猜。
3. 取得同版本 `mm-ipod` ELF 后，确认 transport module name 的加载规则及 plugin init slot；再决定是复用原厂 Bluetooth transport，还是以独立进程/IPC bridge 实现适配。
4. 无论采用哪条路线，先做静态/云端离线测试；不改实车启动参数，不停用 USB transport，不生成安装包。

状态：`RESEARCH_ONLY / NO_INSTALL / safe_to_flash=false / vehicle_verified=false`。
