# R104：MU1005 原厂 iAP2 Bluetooth 组件静态逆向（基于用户上传 ELF）

日期：2026-11-26  
目标文件：`ipod-drvr-iap2(2).so`  
状态：STATIC_ANALYSIS_PARTIAL / NO_INSTALL

## 1. 文件身份（本轮实测）

- Size: 739,925 bytes
- SHA256: `42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf`
- ELF32 little-endian ARM, ET_DYN, EABI5
- Build ID: `f66b11f264233f156e4414ab8b438917`
- Not stripped; 30 ELF sections; 300 dynamic symbols

该哈希与既有 MU1005 原厂审计基线完全一致。

## 2. 已定位的真实函数及地址

| Function | ELF VA | Size | 本轮观察 |
|---|---:|---:|---|
| `iap2_init` | 0x0ea6c | 2220 | 大量读取配置项；需继续解码每个字符串及配置字段 |
| `init_features` | 0x0e2cc | 1952 | 初始化 feature 路径 |
| `bt_update_recv` | 0x0505c | 1100 | 解析收到的结构化更新记录并更新 PPS 状态 |
| `bt_start_updates` | 0x0553c | 372 | 检查状态、准备 iAP2 消息并调用 link_send_message |
| `bt_stop_updates` | 0x054a8 | 144 | 结束更新状态并发送相应消息 |
| `bt_send_info` | 0x056b0 | 328 | 蓝牙信息发送路径（待反汇编） |
| `transport_get_link_params` | 0x21144 | 24 | Transport 抽象接口 |
| `transport_send_pkt` | 0x2115c | 192 | Transport 抽象发送 |
| `transport_recv_pkt` | 0x2121c | 672 | Transport 抽象接收 |

## 3. 对 bt_start_updates 的指令级观察

ARM 反汇编地址范围 0x553c–0x568c：

- 以第一个参数对象为上下文，读取对象偏移 +0x14 的关联对象。
- 检查上下文对象偏移 +0xa4 的 bit 2；未满足时返回错误路径。
- 对关联对象 +0x28 的状态/属性区域执行查询/更新调用。
- 从发送队列取出 packet，读取对象偏移 +0x10c 的 16-bit 值。
- 将该数值作为参数加入消息，随后调用 `link_send_message`，消息 ID 为 `0x4e03`。
- 最后将 packet 放回发送队列。

**可支持的结论：** `bt_start_updates` 是 iAP2 Link 层的一条“启动更新/订阅状态”消息路径；它本身不是 RFCOMM socket 注册函数，也没有直接调用 btstack API 的证据。消息 0x4e03 的具体语义仍需结合对应配置键和上层状态机确认。

## 4. 对 bt_update_recv 的指令级观察

ARM 反汇编地址范围 0x505c–0x5474：

- 参数包含上下文、字节缓冲区和长度；函数先做边界检查。
- 解析小端长度/类型字段；存在 type 0 与 type 1 分支。
- type 1 分支迭代一组 TLV/属性记录，按属性编号映射 10 个状态位。
- 组合属性更新缓冲区，最终调用 `pps_update_bulk` 更新 PPS 对象。
- 未识别类型/字段走错误或丢弃分支。

**可支持的结论：** 这是收到 Bluetooth 状态更新后，将结构化状态写入 PPS 的处理器。当前反汇编片段没有显示它从 RFCOMM 读取原始字节；输入缓冲区的生产者和回调注册位置仍待定位。

## 5. 重要架构修正

此前容易将函数名 `bt_update_recv` 误读为“蓝牙 RFCOMM 数据接收入口”。本轮指令证据更支持它是**蓝牙状态/属性更新接收处理器**，而不是原始 iAP2 帧的物理接收函数。

因此，不能把 R103 LinkEngine 直接挂到 `bt_update_recv`。需要继续追踪：

1. `bt_start_updates` 的调用者和消息 0x4e03 的触发条件；
2. `bt_update_recv` 的注册/调用者，确认上游数据来自哪个 PPS/IPC/BlueSDK callback；
3. `transport_recv_pkt` 的具体 Transport 函数表来源，以及它是否最终进入蓝牙组件；
4. `iap2_init` 中配置字符串和组件选择对象的准确映射；
5. `bt_send_info` 与对应接收函数的配对关系。

## 6. 下一轮云端工作（不需要车辆）

- 导出完整反汇编和重定位，解析 PLT/GOT 调用目标。
- 从 ELF 字符串表精确提取 Bluetooth 相关配置键，并对 `iap2_init` 中 literal pool 引用逐项解码。
- 自动建立所有导出函数的 BL/BLX 交叉引用表，优先定位上述 5 个函数的 caller/callee。
- 分析 `transport_recv_pkt`、`transport_send_pkt` 和 `link_get_transport_data` 的函数表/对象字段。
- 将证据和脚本纳入云端 CI，生成可复核 JSON；测试只做静态分析，不运行 OEM 二进制。

## 7. 安全结论

本轮仅对用户提供的原厂 ELF 做只读静态分析。没有更改原厂程序，没有生成可安装插件，没有证明真实 RFCOMM/iPhone/MFi/Wi-Fi/AirPlay 链路。  
`safe_to_flash=false` / `installable=false` / `vehicle_verified=false`.
