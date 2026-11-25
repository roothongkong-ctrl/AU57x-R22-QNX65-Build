# 原厂输入通知链已定位（第08步）
本轮实际对用户原厂三文件运行 llvm-objdump/readelf（SHA256 与 MM_PEER_CN.md 一致）。
修正上一轮判断：transport_recv_pkt 正是原厂 event 3 输入通知最终调用的驱动槽；packet_recvlist_pop 从已初始化的空闲接收包池取容器，并非要求上游预先填入网络数据。
证据：
- USB usbhost_input 0x3084..0x3094 -> usbhost_upcall event=3 -> 原厂 pulse。
- usbhost_pulse 0x1a10..0x1a24: transport私有对象+0xc -> device; device+0x38 -> driver; driver+0x20 -> callback; r0=driver; blx r3。返回值未使用。
- mm-ipod device_drvr_init 0x103698..0x1036a4: device+0x38=driver, driver+0x14=device。device_transport_init 0x1035b8: device+0x40=transport。
- ipod-drvr-iap2.so iap2_drvr 数据表 0xa9bb0，+0x20=0x2121c (transport_recv_pkt)，对应 R_ARM_RELATIVE relocation。
- transport_recv_pkt 0x21234 -> packet_recvlist_pop 0x1f050（从 driver+0x94 取池头，+0x98 计数，0x1f154 标记2）；0x2126c -> link_get_transport_data；0x126b4 -> transport_receive；0x21124..0x21138 通过 device+0x40/transport+0x2c 调用原始接收槽。
新增 mm_notify 绑定真实原厂导出并核对活动对象之间的指针关系和原厂回调槽；遵循单参数 driver ABI，保持原厂解析与认证，禁止将回调返回值当作认证或数据确认。主机夹具只验证接口行为。仍需在真实 mm-ipod dispatch 上取得当前已初始化 device/driver/transport、建立原厂调度与插件加载；连续多包和原厂缓冲池行为尚需验证。不得安装。
