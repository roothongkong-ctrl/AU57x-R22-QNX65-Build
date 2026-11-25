# MU1005 集中原厂蓝牙研发：原生 EOF 收尾、重连准备及联合 ARM 库
NO INSTALL。复用已通过的 QNX 上层完整串联，不把模拟连接视为真实车机。
- R5 死 socket 不再直接丢弃全部 native event；只接收明确的 close / 匹配的 TX completion，拒绝旧会话数据。close 先到时保留 IN_DRAINING 直到真实 completion；错误状态不强行回收。已完成的 TX 不再引发下一次发送。新的入站申请在已知空闲死服务上拒绝。
- 新增 ir5_drain：原 owner 上先停止传输、请求一次 native disconnect，然后等待真正的关闭、匹配 TX 完成、无挂起请求。无证据不注销。
- mu_bt_native_withdraw 将 drain 与原厂 SDP/RFCOMM 逆序注销串联；au98_rearm_stock_native 必须在 fd 已关闭、独立 SDK 回收证明成立后才释放静态对象。重新 publish 仍需要新的有效 owner 证明与 daemon；没有自动重试、不绕过 MFi。
- 主机测试增加真实 TCP EOF、两个事件顺序、拒绝错误 packet 和旧数据、再用独立 socket 连接。并非真实 SDK 回收证明。
- 将 native_binding、R5、原厂路由选择、Au98 生命周期、EIR 合入同一个 QNX ARM DSO，检查关键导出函数，避免原先只有单独对象却无法联合调用的问题。
- 待验收：本提交电脑端测试和 QNX ARM 交叉编译；实车原厂启动/线程、真实回收证明提供者、真实 Apple MFi、无线媒体仍缺。此前 0x11ec60/0x12a6a0 为原厂静态候选，不自动打补丁。
