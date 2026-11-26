# R105.019｜mm-ipod io_msg 与 OEM device callback 转发 ABI

日期：2026-11-26
目标：MU1005 / QNX 6.5 ARM32
输入：原始交接包 02_原厂二进制_仅私有交接/mm-ipod
Build ID：85ad8230b269addaef1a74ef1e198b71
状态：STATIC_DISASSEMBLY / NO_INSTALL / vehicle_verified=false

## 1. 新发现：mm-ipod 的 Resource Manager handler 是薄转发层

本轮直接反汇编原厂 mm-ipod，确认：

- io_open：VA 0x103980
- io_read：VA 0x1039d0
- io_lseek：VA 0x103a10
- io_notify：VA 0x103a50
- io_unblock：VA 0x103a90
- io_msg：VA 0x1042b4
- io_devctl：VA 0x1043c0
- ipod_resmgr_mount：VA 0x103ad0
- device_transport_init：VA 0x103510

## 2. handler 到设备上下文函数表的偏移

mm-ipod 的全局设备对象通过全局指针取得，随后从对象 +0x38 取出一个函数表/transport context。

实际指令确认：

| Resource Manager handler | 从 context 读取的函数指针偏移 | 说明 |
|---|---:|---|
| io_notify | +0x28 | 条件调用，空指针返回 ENOSYS(值 48) |
| io_unblock | +0x2c | 条件调用，空指针返回 ENOSYS |
| io_open | +0x3c | 条件调用，空指针返回 ENOSYS |
| io_read | +0x40 | 条件调用，空指针返回 ENOSYS |
| io_lseek | +0x44 | 条件调用，空指针返回 ENOSYS |
| io_msg | +0x24 | 处理完分段/缓存逻辑后间接调用 |
 
其中 io_open 入口从参数寄存器保存 r0/r1/r2，之后：
- 从 global_device +0x38 取 context；
- 从 context +0x3c 取 callback；
- callback 非空时将原 handler 参数转交给该 callback；
- callback 为空则返回 48。

io_read 以相同模式调用 context +0x40。

## 3. io_msg 是目前最重要的请求转发入口

VA 0x1042b4，大小 268 bytes。

反汇编路径：
1. 从输入对象读取两个长度/位置字段（+0x20 与 +0x24），处理消息是否需要分段/缓存。
2. 通过 PLT helper 取得/扩展接收数据缓存。
3. 将收到的消息数据复制到缓存，并更新长度/位置。
4. 在数据完整或达到当前处理条件后：
   - 从 global_device +0x38 取得 context；
   - 从 context +0x24 取 callback；
   - 将 context、消息缓存、长度/附加参数交给 callback。
5. callback 返回后释放临时消息缓存。

因此，Resource Manager 的 io_msg 并不直接解释 0x9999 或 0x3000。它负责接收/组装 QNX 消息数据，再把数据交给设备 context 的 +0x24 callback。真正的协议操作码分派很可能位于这个 callback 指向的 OEM device implementation 中，而不是 io_msg wrapper 本身。

## 4. 与 R105.017 客户端请求的关联

R105.017 已确认 libiap2client：
- iap2_connect：open(path) 后 MsgSendv，消息头可见 0x9999；
- iap2_raw_msg_send：MsgSendv，消息头可见 0x3000；
- 两者都是 QNX Resource Manager client/server IPC，不是 RFCOMM socket API。

R105.019 新确认服务端通用 io_msg handler 将消息数据转发给 context +0x24 callback。

下一步要把该 callback 的初始化来源和函数地址找出来，再检查它如何解析 0x9999/0x3000，以及是否调用 driver 的 init/send/receive 路径。

## 5. 重要边界：不能误认 Transport callback

这里 context +0x24 是 mm-ipod Resource Manager/device context 上的 callback。它与 R105.012 中 ipod-drvr-iap2.so 的物理 Transport vtable（+0x18/+0x1c/+0x20/+0x24/+0x28/+0x2c）属于不同对象、不同层级。两者偏移数值相似不代表是同一张表。

## 6. 下一步窄化任务

1. 追踪 mm-ipod 的全局设备对象 +0x38 context 的创建和初始化。
2. 找到 context +0x24 callback 被写入的指令和源函数。
3. 沿 callback 解析输入消息头，确认 0x9999、0x3000 分支。
4. 追踪 callback 与 device_drvr_init、device_transport_init 的调用/函数表关系。
5. 确认 callback 最终如何进入 ipod-drvr-iap2.so 与外层物理 Transport plugin。
6. 最后再匹配 BlueSDK RFCOMM producer；当前仍未证明真实 RFCOMM 数据通道。

## 结论

新增静态证据：mm-ipod 的 io_msg 是 Resource Manager 消息组装/转发 wrapper；实际消息处理交给设备 context +0x24 callback。io_open/read/lseek/notify/unblock 也分别转发至同一 context 的不同 callback 槽。

这把下一步从“猜测哪个 QNX handler 处理 opcode”缩小为“定位 mm-ipod context +0x24 的初始化来源与实现”。

installable=false / safe_to_flash=false / vehicle_verified=false / mfi_apple_verified=false。
