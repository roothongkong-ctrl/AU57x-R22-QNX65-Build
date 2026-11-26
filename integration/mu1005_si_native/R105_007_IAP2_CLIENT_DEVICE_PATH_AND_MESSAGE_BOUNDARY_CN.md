# R105.007｜iAP2 client 设备路径与消息边界继续核对

日期：2026-11-26
目标：MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
状态：STATIC_EVIDENCE_RECONCILIATION / NO_INSTALL / vehicle_verified=false

## 本轮继续推进的三个点

### 1. /dev/ipod0 不是 dio_manager 内部硬编码路径

已有同哈希原厂二进制调用跟踪记录：
- dio_manager SHA256：e40b5bf9df3f96221ac01bc0484e06493e6c58e60cca32d2547939ff1d56f63c
- iap2_connect PLT：0x001127f0
- iap2_connect 调用点：0x0017a094
- 调用包装函数：0x0017a054
- 包装函数由 0x0017a614 调用；当时寄存器跟踪显示 path 参数由 caller 传入（R11 所指内联/间接字符串）。

同一版本的原厂 dio_manager 配置中，iap2.device 为 /dev/ipod0。当前最符合证据的解释是：配置中的 device 字段被读取后，传入 iAP2 connect wrapper；不是二进制把 /dev/ipod0 写死。

仍未闭环：还没有从 JSON/配置对象解析函数一直追到 0x17a614 的字段读取指令，也没有排除启动环境覆盖或配置 fallback。故字段到调用实参的对应关系为高概率，不标成完整静态证明。

### 2. iap2_connect 与 iap2_raw_msg_send 是不同层级的 client API

原厂 libiap2client.so.1 导出：
- iap2_connect（VA 0x2cec）
- iap2_disconnect（VA 0x2c58）
- iap2_raw_msg_send（VA 0x131c）
- iap2_eap_send（VA 0x13b8）
- iap2_bt_status_set（VA 0x1954）
- iap2_eap_event_arm（VA 0x1efc）
- iap2_eap_file_descriptor（VA 0x127c）

该库依赖 libstrm.so.1 与 libc.so.3，其通信路径使用 QNX 消息传递（已有静态导入证据包括 MsgSend / MsgSendsv / MsgSendvs）。

关键边界：
- iap2_connect(path,...) 是建立 client 到设备服务的连接入口。
- iap2_raw_msg_send 是连接建立后的 client API，名称中的 raw_msg 不等于“任意 RFCOMM 字节直接注入物理 Transport”。
- 仅凭导出符号和地址，无法判断它发送的是 iAP2 上层消息 envelope、某个 client 命令结构，还是可透明承载任意 iAP2 packet 的 payload。
- 也不能把它等同于 driver 内部的 transport_send_pkt / transport_recv_pkt。

因此，本轮撤销“把 iap2_raw_msg_send 当作最小 RFCOMM 数据桥已知入口”的过度简化表述。它是值得反汇编的 client 入口，但还不是已证实的任意字节桥。

### 3. 当前最可能的数据路径（分层表达）

```
DIO config: iap2.device=/dev/ipod0
            |
            v
dio_manager caller / wrapper (0x17a054)
            |
            v
libiap2client.so.1 : iap2_connect (0x2cec)
            |
            | QNX MsgSend-family IPC (client/server request protocol)
            v
QNX Resource Manager device endpoint (driver module hosted by mm-ipod; runtime PID/path not verified)
            |
            v
ipod-drvr-iap2.so iAP2 state / transport component dispatch
            |
            +---- BluetoothTransportComponent (static strings/symbol evidence; runtime not proven)
            |
            +---- USB physical Transport interface (OEM ABI verified; startup selects usbdevice)
```

这张图把 client IPC 与 iAP2 内部 transport 分开。不能从 iap2_connect 成功或 iap2_raw_msg_send 存在推断蓝牙 RFCOMM 已到达 driver。

## 当前证据分级

| 结论 | 状态 | 说明 |
|---|---|---|
| dio_manager 使用 libiap2client | 静态确认 | NEEDED/import/PLT callsite |
| connect 路径由 caller 参数传入 | 静态确认 | wrapper 调用寄存器/参数跟踪 |
| 配置值 /dev/ipod0 是 path 来源 | 高概率 | 配置存在且参数流吻合；配置解析至 callsite 尚未闭环 |
| client 使用 QNX MsgSend-family IPC | 静态确认 | libiap2client imports |
| Resource Manager 在 mm-ipod 模块宿主链 | 高置信架构推导 | R105.006；尚无 live PID |
| iap2_raw_msg_send 发送任意原始 RFCOMM 帧 | 未证明 | 必须反汇编参数与消息封装 |
| 蓝牙 RFCOMM 数据进入 driver 的 BluetoothTransportComponent | 未证明 | 缺真实 producer/consumer 调用链和 OEM runtime |
| 原厂 USB 插件与蓝牙组件可并行运行 | 未证明 | 不得修改 -t usbdevice |

## 下一轮按优先级继续做

1. 对 libiap2client.so.1 的 iap2_connect、iap2_raw_msg_send、iap2_disconnect 分别恢复 ARM 调用约定、参数长度、请求结构、MsgSend 收发缓冲区与返回值语义。
2. 将 client 请求 opcode 与 ipod-drvr-iap2.so 的 Resource Manager dispatch 表逐项匹配；找到服务端 handler 后，确定 connect 句柄与 session 生命周期。
3. 对 dio_manager 的 0x17a614 caller 向上追踪配置字段读取，确认 /dev/ipod0 来自 iap2.device 的完整数据流。
4. 独立分析 driver 中 transport_send_pkt、transport_recv_pkt、bt_start_updates、bt_update_recv 的函数引用与调用关系；不要把字符串命中当作调用图。
5. 若二进制未包含在可访问工作区，明确索取的最小文件为原厂 libiap2client.so.1、ipod-drvr-iap2.so、dio_manager 的同版本原始 ELF；此前用户已上传或归档的文件不重复索取。

## 安全边界

不改 iap2.cfg、DIO 配置、启动脚本或 USB Transport；不伪造认证，不抓取认证秘密，不做安装。任何 host/QEMU 结果必须标注 HOST/QEMU，不能冒充 OEM runtime。

**结论：client 设备路径与 API 分层更清楚了；真正缺的不是再造一个 bridge，而是把 QNX client IPC request 和 driver Resource Manager handler 的 ABI 对上。**

installable=false / safe_to_flash=false / vehicle_verified=false。
\n\n## R105.008 架构分层复核\n\n后续交叉核对已明确区分：mm-ipod 外层物理 Transport 插件、ipod-drvr-iap2.so 内部 iAP2 TransportComponent、btstack 的 RFCOMM producer/owner 三个层级。driver 内部出现 BluetoothTransportComponent 不等于存在可直接选择的外层 bluetooth 插件。当前研发策略为复用 R103 的 iAP2 LinkEngine、沿用 R98/R45 原厂接入层，保留原厂 usbdevice 和有线功能。详见 [R105.008 路线分层复核](R105_008_ROUTE_LAYER_RECONCILIATION_CN.md)。\n