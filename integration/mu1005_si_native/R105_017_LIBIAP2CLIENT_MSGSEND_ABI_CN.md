# R105.017｜libiap2client.so.1 MsgSendv ABI 反汇编记录

日期：2026-11-26
目标：MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
状态：STATIC_DISASSEMBLY / NO_INSTALL / vehicle_verified=false

## 本轮直接使用的原厂 ELF

工作区中的原始文件：
- libiap2client.so.1（ARM32 EABI5，未剥离）
- ipod-drvr-iap2.so（ARM32 EABI5，未剥离）
- dio_manager（ARM32 EABI5，strip）
- mm-ipod（ARM32 EABI5，未剥离）

本报告仅记录对 libiap2client.so.1 的实际反汇编结果，不把函数名或字符串当作运行时证明。

## 1. iap2_raw_msg_send：已确认构造固定 IPC 消息并调用 MsgSendv

函数地址：0x131c，大小 0x9c。

ARM 调用约定的入口寄存器使用：
- r0：指向 client 句柄/连接对象（函数后续从该对象偏移 0 取出 fd/连接描述符）
- r1：数据指针（保存至栈上 iov 数据指针槽）
- r2：数据长度（保存至栈上长度槽；零长度或空指针会 EINVAL）

函数在栈上构造消息头，观察到的常量：
- 0x0113：消息类别/服务标识字段（具体语义尚未由服务端 dispatch 对照确认）
- 0x0013：固定字段
- 0x3000：命令/操作字段候选（需要结合 QNX MsgSendv 的 IOV 结构布局和 server dispatch 确认）
- 0x0014：固定字段

然后调用 PLT 入口 0x1134，静态重定位表确认该 PLT 目标为 MsgSendv，传入 2 个 iov。由此可确认：
- 该 API 不是直接调用 write()/send() 将字节写入 RFCOMM socket；
- 它把调用参数封装成 QNX 消息，通过 MsgSendv 交给 fd 对应的服务端；
- r1/r2 数据参数进入消息 iov 的数据部分；
- 因为 server handler 尚未与 opcode 对齐，不能断言该 payload 是完整 iAP2 wire packet、应用层消息，或仅仅是 client 命令参数。

## 2. iap2_connect：路径参数进入 open，随后执行 MsgSend 请求

函数地址：0x2cec，大小 0x114。

可见调用关系：
- r0 路径参数、r1 第二参数先保存；
- 调用 open(path, flags) 的 PLT 入口 0x114c；
- open 返回的 fd 被保存到新建 client 对象；
- 随后构造本地请求消息，包含 0x0113、0x0013、0x9999 等字段；
- 调用 PLT 入口 0x102c；
- 重定位表确认 0x102c 对应 MsgSendv；
- 请求后读取返回结构中的状态字段；状态非预期会关闭 fd 并释放对象。

因此 /dev/ipod0 这种路径的作用是打开 QNX Resource Manager 设备端点；connect 不是 TCP connect，也不是 RFCOMM 建链。它建立的是 client 到该设备服务的 QNX 消息通道。

## 3. iap2_disconnect

函数地址：0x2c58，大小 0x30。

观察到：
- 从 client 对象取出偏移 +4 的资源句柄并释放；
- 对偏移 0 的 fd 调用 close；
- 释放 client 对象。
这支持 connect 返回对象持有 fd 和附属资源的判断；偏移 +4 的对象具体类型仍需追踪分配路径。

## 4. 当前能确认的端到端层次

```
dio_manager
  |
  | iap2_connect(path, ...)
  v
libiap2client.so.1
  | open(path)
  | MsgSendv(connect request)
  v
QNX Resource Manager endpoint (e.g. /dev/ipod0)
  |
  | server-side request dispatch not yet matched
  v
ipod-drvr-iap2.so / mm-ipod host chain (static architecture evidence)
  |
  | internal iAP2 TransportComponent
  v
physical transport producer/consumer (Bluetooth RFCOMM ownership still unproven)
```

## 5. 下一步明确缩小为服务端 opcode/IOV 对照

优先从以下点推进：
1. 在 ipod-drvr-iap2.so 与 mm-ipod 中定位 Resource Manager attach、iofunc handler、read/write/devctl/io_msg dispatch 的真实调用点。
2. 将 connect 请求中的 0x9999 与 raw_msg_send 请求中的 0x3000 对照服务端消息处理分支。
3. 恢复 MsgSendv 的 send IOV 数组布局及 reply buffer 长度，确认数据长度字段究竟位于第几个 iov。
4. 追踪 server reply 的结构和错误码，确认 connect 返回状态字段的语义。
5. 再从服务端 handler 追踪到 driver transport_send_pkt / transport_recv_pkt 或其他内部数据入口。

## 6. 严格结论

已确认：libiap2client 的 raw_msg_send 与 connect 都使用 QNX MsgSendv IPC；connect 先 open 设备路径；raw_msg_send 将数据参数封装进 IPC 消息，而非直接写 RFCOMM socket。

尚未确认：0x3000/0x9999 的真实 opcode 名称、服务端 dispatch 分支、payload 是否为完整 iAP2 帧、RFCOMM producer 是否连接到该服务端。

不修改车机配置，不修改原厂二进制，不安装。

installable=false / safe_to_flash=false / vehicle_verified=false / mfi_apple_verified=false。
