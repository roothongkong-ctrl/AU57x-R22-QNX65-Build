# 第08步：mm-ipod 到 DIO 双向本地数据通道
已核对本地原厂文件（未上传）：mm-ipod SHA256 4b87cd2d41ddcee2f38800feafe8b4b340e4f23e1d86c7e97af303522aa15d09，ipod-drvr-iap2.so 42fd984ebbdccd11595bd45909683829565147c8cbc8a482618d6dd8e564d3bf，ipod-transport-usbhost.so be3d8f4076d03408624415b8e324ca70ccc437dc7d0894f0eadbd356e5b410a0。
原厂 transport_send_pkt 0x211e0..0x21204 调用 transport+0x28 并比较返回值与原始包长；transport_receive 0x2111c..0x21140 调用 transport+0x2c（timeout=0，第五参数=0）。USB host receive 0x1208..0x121c 在无数据且 timeout=0 时返回0。新增两个匹配签名的适配函数，发送整包入队、非阻塞流发送、部分读取及断线处理。整包入队不表示对端收到；不伪造认证或 ACK。
重要：既有 R141 r129_driver_call.c 把 transport_recv_pkt 作为 input_ready 候选，但该函数 0x21234 先调用 packet_recvlist_pop，未证明能通知新数据。原厂 USB host 输入 0x3084 调用 usbhost_upcall(event=3)，0x2a58..0x2a6c 走通知 pulse；不能直接把旧 callback 复制过来。新模块要求真正的 input-ready 通知适配，下一步查明原厂 dispatch 入口。
既有 R29/R141 蓝牙 transport 使用其专属帧协议及类型3，不能直接拿来传第二段 iAP2 原始流，也不能假定类型3适用于第二段。当前新增仅数据接口，不导出 ipod_module，不替换原厂 USB 插件，不提供设备所有权或授权。主机真实 socketpair 双端测试；尚非原厂进程。
