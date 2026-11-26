# R105.018｜mm-ipod Resource Manager 注册与请求分发 ABI

日期：2026-11-26
目标：MU1005 / QNX 6.5 ARM32
证据来源：用户此前完整交接包中的原厂 ELF；本轮对实际文件重新执行 readelf 与 ARM 反汇编
状态：STATIC_DISASSEMBLY / NO_INSTALL / vehicle_verified=false

## 1. 原厂二进制身份与可见符号

本轮工作区复核文件：
- mm-ipod：ELF32 ARM EABI5，Build ID 以当前交接包为准
- ipod-drvr-iap2.so：ELF32 ARM EABI5，Build ID f66b11f264233f156e4414ab8b438917
- libiap2client.so.1：ELF32 ARM EABI5，Build ID 531ab3194365f4e9fd2236664ccf225e

mm-ipod 可见关键符号：
- ipod_resmgr_mount：VA 0x00103ad0，512 bytes
- io_open：VA 0x00103980，80 bytes
- io_read：VA 0x001039d0，64 bytes
- io_devctl：VA 0x001043c0，184 bytes
- device_transport_init：VA 0x00103510，228 bytes
- mountpoint：VA 0x00107fb0，24 bytes

导入符号包含 dispatch_create、dispatch_handler、dispatch_context_alloc/free、dispatch_block、resmgr_attach、iofunc_func_init、iofunc_attr_init、iofunc_open_default、iofunc_devctl_default、thread_pool_create/start 等 QNX Resource Manager API。

## 2. ipod_resmgr_mount 的实际反汇编证据

函数入口 0x103ad0：
- r0 保存为 r6（mount/context 对象）
- r1 保存为 r7（第二参数，函数后续将其作为是否提供自定义路径/名称的分支条件使用）
- 在 0x103b20 调用 iofunc_func_init，初始化 connect/io function 表。
- 函数将 mm-ipod 内部处理函数地址写入 resmgr I/O function table：
  - 0x103980（io_open）写入 attr +0x24
  - 0x1039d0（io_read）写入 attr +0x48
  - 0x103a10、0x103a50、0x103a90 等本地处理函数写入相应槽位
  - 0x1042b4、0x1043c0（io_devctl）写入 devctl 等槽位
- 0x103b40 调用 iofunc_attr_init 初始化资源属性。
- 函数后段从 r6+0x20 读取 mount path/name 参数，并构造 resmgr_attach 的参数。
- 0x103c90 调用 resmgr_attach；返回值为 -1 时记录错误并返回失败，否则返回成功。

由此确认：mm-ipod 确实创建 QNX Resource Manager endpoint，并将 mm-ipod 自己的 io_open/io_read/io_devctl 等 handler 注册到该 endpoint。它不是仅仅通过字符串或配置猜测出来的服务端。

注意：函数内部还包含根据 r6+4 和 r7 选择默认/自定义路径的分支。仅凭本次片段不把默认路径字面值或 dio_manager 的配置字段来源标为完全闭环；需继续把 mount 调用者和 r6 结构体构造向上追踪。

## 3. 客户端与服务端现已能连接到同一 IPC 模型

libiap2client.so.1 的静态反汇编记录（R105.017）确认：
- iap2_connect 先 open(path)，再对 fd 调用 MsgSendv；
- iap2_raw_msg_send 也通过 MsgSendv 将请求发给该 fd；
- 因此它们属于 QNX Resource Manager client/server 消息路径，而不是 RFCOMM socket API。

本轮在服务端补上了真实注册证据：
- mm-ipod：resmgr_attach 注册 endpoint；
- mm-ipod：iofunc_func_init 设置 handler table；
- mm-ipod：io_open/io_read/io_devctl 为实际 handler；
- ipod-drvr-iap2.so：提供 iAP2 driver 内部处理与 Transport callback table；
- libiap2client.so.1：作为打开 endpoint 并发送 IPC 请求的 client。

## 4. 尚未完成的关键 ABI 对照

本轮尚未证明：
- 0x9999 connect request 具体落到哪个服务端 io_msg/devctl 分支；
- 0x3000 raw_msg_send request 的服务端 handler 和完整 IOV 布局；
- mm-ipod handler 如何转发至 ipod-drvr-iap2.so 的具体函数；
- BluetoothTransportComponent 是否由真实 BlueSDK RFCOMM producer 驱动；
- OEM runtime 中 endpoint 的最终路径、进程实例和权限。

特别说明：io_open/io_read/io_devctl 注册证据并不意味着 0x9999/0x3000 已经完成 opcode 对照；必须继续检查 resmgr_attach 的 dispatch function table、io_msg handler，以及 mm-ipod 对 driver 的 ioctl/devctl/write/read 转发。

## 5. 下一步（窄化任务）

1. 反汇编 mm-ipod 的 io_open/io_read/io_devctl 和相邻未命名 io_write/io_msg handler，恢复请求类型与参数结构。
2. 追踪 resmgr_attach 注册的 connect_funcs/io_funcs 全部槽位，建立 QNX 消息类型到 handler 的映射。
3. 对照 libiap2client 的 0x9999、0x3000 两个请求，匹配 server handler 与 reply 结构。
4. 追踪 mm-ipod 对 ipod-drvr-iap2.so 的 dlopen/符号解析/driver init 和 read/write/devctl 转发。
5. 再连接到 driver 的 transport_send_pkt/transport_recv_pkt 与外层 Transport callback owner。

## 严格结论

本轮新增的真实突破：mm-ipod 的 ipod_resmgr_mount 内部确实调用 iofunc_func_init 与 resmgr_attach，并把 io_open/io_read/io_devctl 等处理函数写入 QNX Resource Manager 函数表。这是服务端 endpoint 注册的静态反汇编证据。

仍未完成：客户端 opcode 到服务端 handler 的精确映射，以及服务端到 RFCOMM producer 的数据通道。

不修改车机、不替换原厂二进制、不安装、不触碰认证秘密。

installable=false / safe_to_flash=false / vehicle_verified=false / mfi_apple_verified=false。
