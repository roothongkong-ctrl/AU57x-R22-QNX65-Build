# R104 原厂 iAP2 BluetoothTransportComponent 反汇编初步结论

日期：2026-11-26  
对象：用户本轮上传的 `ipod-drvr-iap2(2).so`  
SHA256：`42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf`  
文件大小：739925 bytes；ELF32 ARM EABI5 ET_DYN；未剥离符号（not stripped）。

## 本轮直接反汇编得到的函数地址

| 符号 | ELF VA | 大小 |
|---|---:|---:|
| `bt_update_recv` | 0x505c | 0x44c |
| `bt_start_updates` | 0x553c | 0x174 |
| `bt_send_info` | 0x56b0 | 0x148 |
| `init_features` | 0xe2cc | 0x7a0 |
| `iap2_init` | 0xea6c | 0x8ac |
| `link_create` | 0x11da0 | 0x468 |
| `link_setup` | 0x12888 | 0x438 |
| `transport_receive` | 0x2111c | 0x28 |
| `transport_get_link_params` | 0x21144 | 0x18 |
| `transport_send_pkt` | 0x2115c | 0xc0 |
| `ipod_module` | 0x27e7c | 0x24 |

## 关键反汇编发现

1. `iap2_init` (0xea6c) 解析配置/初始化参数，成功路径明确调用 `init_features` (0xe2cc)，随后进入 iAP2 功能初始化；这再次证明不能跳过 `init_features`。
2. `bt_start_updates` (0x553c) 并非一个可直接认定为 RFCOMM 原始字节接收函数。其代码操作 packet send-list，并通过 `link_send_message` 发送消息 ID `0x4e03`；因此更像 iAP2 链路层的 Bluetooth 状态/信息更新发送入口。实际启动语义仍需结合调用者与消息定义确认。
3. `bt_send_info` (0x56b0) 组装一个 11 字节 blob 参数，并通过 `link_send_message` 发送消息 ID `0x4e01`。它是 iAP2 link message 发送路径，不是 RFCOMM socket 写函数。
4. `bt_update_recv` (0x505c) 解析传入 buffer 中的结构化记录，随后多次调用 `pps_buffer_concantenate` 和 `pps_update_bulk` 更新 PPS 状态。由此可确认它是结构化 Bluetooth 状态更新处理函数；不能把它误认作 RFCOMM 原始字节入口。
5. `transport_receive` (0x2111c)、`transport_send_pkt` (0x2115c) 是独立的通用 transport 抽象入口。已见 `link_create` 调用 `transport_get_link_params`，以及后续链路路径调用 `transport_receive`。下一步应追踪 transport 对象的函数指针/owner 来源，而不是把 bt_* PPS 状态处理函数接入 socket。
6. 当前可见的 `bt_start_updates`、`bt_send_info` 对 `link_send_message` 的调用表明它们工作在 iAP2 link 消息层；RFCOMM 物理传输仍需沿通用 transport 对象追踪。

## 本轮结论与下一步

已经从符号表和指令级代码把几个关键名字从“字符串线索”推进为可定位的函数实现。最重要的修正是：`bt_start_updates` / `bt_update_recv` / `bt_send_info` 属于 Bluetooth 信息/状态与 iAP2 link message 路径，不能直接当成 RFCOMM 数据收发回调。

下一步优先反汇编 `link_create`、`link_setup`、`transport_receive`、`transport_send_pkt`，恢复 transport vtable 的真实字段与调用者；并分析 `iap2_init` 如何将配置组件绑定到 transport。随后再与 mm-ipod USB transport ABI 对照。未完成真实蓝牙物理 producer 绑定前不生成可刷写程序。

状态：`RESEARCH_ONLY / NO_INSTALL / safe_to_flash=false / vehicle_verified=false`。


## R104 第二轮：transport vtable 调用约定已从机器码确认（2026-11-26）

使用本轮上传且 SHA256 精确匹配的 ELF 原件完成 ARM32 反汇编：

- `link_create` VA `0x11da0`，大小 `0x468`；入口直接调用 `transport_get_link_params(link, &params)`。
- `link_setup` VA `0x12888`，大小 `0x438`；再次读取 transport link params，之后建立 iAP2 session、构造并发送 SYN。
- `transport_receive` VA `0x2111c`：从 `link+0x14` 取 link-private object，再从 private+`0x40` 取 transport object；从 transport object+`0x2c` 取函数指针并 BLX 调用。故接收回调槽偏移可确认为 transport vtable/object 的 `+0x2c`。
- `transport_get_link_params` VA `0x21144`：同一 transport object 的 `+0x18` 位置是 get-link-params 函数指针。
- `transport_send_pkt` VA `0x2115c`：调用 send 路径时将 link-private object 的 `+0x48` 字段作为状态/速率门限读取；它本身没有直接通过 transport vtable 的可见 send slot 完成发送，需继续追踪后半段和其 caller。
- 全 ELF 直接 BL 调用 xref：`transport_send_pkt` caller 至少在 `0x1127c` 与 `0x11980`；`transport_receive` caller 在 `0x126b4`；`transport_get_link_params` caller 在 `0x11e0c` 与 `0x128a4`。

### 当前能确认的 ABI 形态（不是完整结构体）

- transport object 指针位于 link-private+`0x40`。
- transport.get_link_params 函数指针位于 transport+`0x18`。
- transport.receive 函数指针位于 transport+`0x2c`。
- 仍未从该 ELF 单独确认 transport.send 对应的 vtable slot 偏移、对象创建者、Bluetooth 实例具体绑定点；不可自行补齐为相邻偏移。

### 结论更新

现在已找到 iAP2 协议引擎调用物理 transport 的真实间接调用边界：接收数据必须由 transport object 的 receive slot（+0x2c）交给 link；链路参数由 slot（+0x18）提供。下一步不是继续改 `bt_update_recv`，而是沿 link-private 的 transport object 初始化/赋值 xref 回溯其 producer，并找出 BluetoothTransportComponent 如何填充该对象及 send 对应回调。USB 插件 ABI 可作参照，但还不能宣称蓝牙绑定已找到。


## R104 第三轮：确认 send vtable slot，完成收发双向 ABI（2026-11-26）

本轮直接使用上传 ELF 的 llvm-objdump 指令输出，纠正此前“send slot 尚未确认”的状态：

- `transport_send_pkt` (0x2115c) 在函数尾部明确执行：从 link-private `+0x40` 取 transport object，再从 transport object `+0x28` 取函数指针并 BLX 调用；参数为 transport/private context、packet data pointer、packet length。
- `transport_receive` (0x2111c) 从同一 transport object `+0x2c` 取函数指针并 BLX 调用。
- `transport_get_link_params` (0x21144) 从同一 transport object `+0x18` 取函数指针并 BLX 调用。

### 当前已确认的最小 transport vtable

| 偏移 | 槽位用途 | 证据函数 |
|---:|---|---|
| +0x18 | get_link_params | transport_get_link_params |
| +0x28 | send | transport_send_pkt |
| +0x2C | receive | transport_receive |

这三个槽位来自同一 transport object，且 send/receive 两条路径均由 iAP2 引擎间接调用。由此可以把“协议引擎所要求的底层物理 transport ABI”具体化，不再只是接口推测。

### 尚未完成

- 谁分配并填充 transport object；
- BluetoothTransportComponent 实际如何构造这个 object；
- callback owner/thread 与 RFCOMM socket/BlueSDK 的关系；
- USB transport 的函数表是否布局完全相同（必须读取 USB ELF 代码验证，不能仅凭名字推断）。

下一步沿 `link_create` 中的 link-private 初始化字段、`ipod_module` 入口及 `iap2_init` 配置解析回溯 transport object producer，并对照 USB transport ELF。保持 NO_INSTALL。


## R104 第四轮：transport object 的分配/注入边界（2026-11-26）

本轮直接复核上传 ELF 原件（SHA256 与预期一致），并对 `link_create` 完整函数反汇编。

### 新确认

1. `link_create` (0x11da0) 接收的第一个参数是 link/config context；函数通过 context+`0x14` 访问 owner/context，再从 owner+`0x20`、owner+`0x24` 取值用于初始化 link/session。该函数内未发现对 BluetoothTransportComponent 构造函数的直接调用。
2. `link_create` 中调用 `transport_get_link_params` (0x21144)，随后将 link-private 状态对象挂到 link 的 `+0x14` 字段。协议引擎的物理 transport 指针不是在 `transport_receive` 内创建，而是由 link-private+`0x40` 预先提供。
3. `transport_send_pkt` (0x2115c) 机器码明确从 link-private+`0x40` 取得 transport object，再从 transport+`0x28` 取 send callback 并 BLX；参数为 transport object、packet data、packet length。发送成功条件为返回字节数等于 packet length。
4. `transport_receive` (0x2111c) 同样从 link-private+`0x40` 取得 transport object，再从 transport+`0x2c` 调用 receive callback。
5. `transport_get_link_params` (0x21144) 使用 transport+`0x18` callback。

### 对当前问题的意义

iAP2 协议引擎不负责创建蓝牙 socket，也不负责寻找 BluetoothTransportComponent。它只消费由上层准备好的 transport object。真正要定位的 producer 应在 `iap2_init` / config component setup 或外部 owner 对 link-private context 的初始化中，而不是继续在 `transport_send_pkt` 内找 RFCOMM 注册。

### 尚未完成的反向追踪

- link-private+`0x40` 的赋值指令/上层对象创建者尚未从当前单个 ELF 完整定位；需要进一步追踪 link-private allocation/initialization 的调用路径以及相关函数指针表。
- 当前 ELF 未提供可直接证明 BluetoothTransportComponent 的构造函数/注册入口；字符串存在不等于构造调用已找到。
- 尚未证明 Bluetooth 组件的回调由 BlueSDK、btstack 还是独立 IPC owner 提供。

### 下一步

优先分析 `iap2_init` (0xea6c) 中 config parser 到 link context 的对象传递，并对 `link_get_transport_data` (0x12208) 及其 callers 做完整交叉引用；若 producer 位于独立 `mm-ipod`/transport plugin，则必须取得相应同固件 ELF，不能凭当前驱动库猜测。继续 NO_INSTALL。
