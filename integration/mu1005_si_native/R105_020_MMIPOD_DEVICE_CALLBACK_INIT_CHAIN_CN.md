# R105.020｜mm-ipod device callback 初始化链复核

日期：2026-11-26
目标：MU1005 / QNX 6.5 ARM32
原厂文件：交接包 mm-ipod
Build ID：85ad8230b269addaef1a74ef1e198b71
状态：STATIC_DISASSEMBLY / NO_INSTALL / vehicle_verified=false

## 1. 本轮重新核对的直接证据

原始反汇编文件：continuation-audit/mmipod_disassembly.txt 与 mmipod_disasm_for_r43.txt；两份文件中对应地址的指令一致。并以原始 ELF 的 file/Build ID 核对目标身份。

关键函数：
- ipod_main：初始化入口，函数主体约 0x103d?–0x1042b0
- device_transport_init：0x103510
- device_drvr_init：0x1035f4
- device_acp_init：0x1036bc
- ipod_resmgr_mount：0x103ad0
- io_msg：0x1042b4
- io_devctl：0x1043c0

## 2. ipod_main 的初始化顺序（指令级 callsite）

在 ipod_main 中可见：
1. 0x104168 调用 ipod_resmgr_mount(global_device, 0)。
2. 0x104170 从 global_device +0x20 取得路径/名称指针。
3. 0x104178 调用外部 helper，参数 512。
4. 0x10417c 将返回值写入 global_device +0x24。
5. 0x1041b8 调用 device_acp_init(global_device)。
6. ACP 初始化返回成功后，0x1041f4 调用 device_drvr_init(global_device)。
7. Driver 初始化返回成功后，0x104230 调用 device_transport_init(global_device)。
8. Transport 初始化返回成功后进入后续主循环/事件处理路径。

这建立了 mount、ACP、driver、transport 的实际初始化先后关系。它并不单独证明每个 callback 槽由哪个函数写入，需结合各 init 函数内部和其调用的 iface initializer 继续追踪。

## 3. device_transport_init 的真实接口发现路径

0x103510 的反汇编显示：
- 从传入 device 对象 +0x1c 读取接口名称/标识字符串；
- 若非空，先调用字符串长度 helper；
- 构造包含接口名及相关参数的本地结构；
- 调用 iface_find(device, "Transport", local_query)；
- 将返回 iface 指针保存到 device +0x40；
- 若查找失败，读取全局错误状态并返回；
- 若成功，从 iface +0x14 取函数指针，调用该函数并传入接口标识字符串和本地 query 结构。

因此可确认：mm-ipod 的 transport 初始化不是简单地直接创建一个 RFCOMM socket；它先通过 iface_find 查找名为 Transport 的接口，再调用该接口表 +0x14 的初始化入口。具体 iface 实例、+0x14 的实现地址、配置筛选结果仍需从 iface_find 和插件注册表闭环。

## 4. R105.019 的 io_msg 转发证据如何接入本轮

R105.019 已从 io_msg 反汇编确认：
- 消息数据被接收到/组装进缓存；
- 随后从 global_device +0x38 取得设备上下文；
- 从该 context +0x24 取 callback；
- callback 收到 context、消息对象/缓存、长度及附加参数。

注意 global_device +0x24（ipod_main 中保存 mount/路径 helper 返回值）与 global_device +0x38 指向的 context 内部 +0x24 callback 是不同对象/不同层级，不能混淆。

本轮没有直接找到 context +0x24 callback 的写入点，故不声称 0x9999 或 0x3000 已经匹配到某个 handler。

## 5. 目前能形成的初始化和消息路径

```
ipod_main
  |
  +-- ipod_resmgr_mount(global_device, 0)
  |      +-- resmgr_attach / QNX Resource Manager handlers
  |
  +-- device_acp_init(global_device)
  |
  +-- device_drvr_init(global_device)
  |
  +-- device_transport_init(global_device)
         +-- iface_find(device, "Transport", query)
         +-- iface + 0x14 -> transport interface initializer

QNX client (libiap2client)
  |
  +-- open(device path)
  +-- MsgSendv(connect / raw-msg requests)
  |
Resource Manager io_msg
  |
  +-- assemble message buffer
  +-- device context + 0x24 callback
       [callback writer and opcode parser not yet identified]
```

## 6. 下一步精确任务

1. 反汇编 iface_find，恢复其注册表遍历、名称匹配、版本/能力筛选规则。
2. 找出 iface 注册表构造函数和每个 iface 的函数表地址。
3. 追踪 device context +0x24 callback 的实际写入指令，确认属于 ACP、driver 还是 transport 初始化。
4. 反汇编该 callback，检查 0x9999 与 0x3000 是否作为 QNX message command 被分派。
5. 将 callback 的调用链与 device_drvr_init / ipod-drvr-iap2.so 入口关联。
6. 独立追踪 Transport iface +0x14 初始化器和 BluetoothTransportComponent/BlueSDK RFCOMM producer 的关系；不能把 Transport interface initializer 误认为已经找到 RFCOMM。

## 严格结论

新增确认：
- ipod_main 实际先 mount Resource Manager，再初始化 ACP、driver、transport；
- device_transport_init 通过 iface_find 查找 "Transport" 接口，并调用接口表 +0x14 初始化入口；
- R105.019 的 io_msg 转发 callback 与 transport iface 是不同对象层级。

仍未确认：
- context +0x24 callback 的写入点/实现；
- 0x9999/0x3000 的服务端命令分派；
- Transport iface 的实际插件实例及其与 BlueSDK RFCOMM 的数据接线。

installable=false / safe_to_flash=false / vehicle_verified=false / mfi_apple_verified=false。
