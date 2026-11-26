# MU1005 统一研发线（NO INSTALL）
唯一工作分支 mu1005-si-cprx-real-review，main 不改动。基线 c5e16ed5 已通过 QNX ARM SI/CPRX 与上游真实 MFi wrapper 编译。
本次同步最近 R29 双路线包的类型3 profile 源码、正反例测试；新 ARM 产物仅为 profile 对象，不是完整 mm-ipod 插件。原厂驱动 type_get +0x1c 判定 type==3，功能位4仍需启用；16字节原厂默认值不等于已验证蓝牙调参。
同一 workflow 并行执行：原厂传输、R99 配置、认证、双频策略、媒体、SD事务回归；另执行锁定 LuKa graft 旧适配，使用 deny-only owner，不接入原厂运行进程。最终收集每路日志、ARM 产物和任务状态、SHA256。现有 GNU 源码缓存沿用。旧任务单独 SI/CPRX 主机回归保留。
原厂 /pps/services/multimedia/iap2/ Authenticated 需要与实际当前 owner 会话绑定；WiFi enable 走 0x5703，不等于完整无线CarPlay协商；[bluetooth] 主要状态报告。congobg donor 缺 11 个 DIO 原厂符号（9个 HID/Screen），不得直接替换原厂媒体库。保留有线、首连2.4GHz优先、后续5GHz。未将原厂/第三方二进制提交 GitHub。
本次是统一工程与并行编译准备，不是六个功能均已完成；完整插件的 owner、清理、WiFi会话、真实MFi和最终安装仍未实现。SD事务回归不是安装程序。


## R105.005 跨分支统一复核（2026-11-26）

已核对 R88–R103、R142/R143、原厂 ASI Bluetooth iAP proxy、SI/DIO CPRX、AirPlay 第二段 iAP2 和 AP live。重要修正：不再修改 `iap2.cfg` 的 `[bluetooth]` 状态段；仅保留 `bluetooth.enableIap` 为待验证配置候选。下一重点是 ASI proxy 9 个方法的真实调用链，以及 `AirPlayReceiverSessionSendCommand` 对应 event/control dispatch owner。R143 线程观察和 QEMU 合成调用不能当作 OEM owner 证明。用户频段要求固定为 5GHz 优先、不可用回退 2.4GHz。详见 [R105.005 路线收敛报告](R105_005_ROUTE_RECONCILIATION_CN.md)。NO_INSTALL。
