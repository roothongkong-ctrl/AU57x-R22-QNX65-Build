# MU1005 原厂启动与蓝牙广播突破（NO INSTALL）
原始用户上传 btstack SHA256 51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566，1846436 bytes；实际 ELF 42 项地址/分支/字面量/PT_LOAD 检查通过；一字节变更被拒绝。仅提交静态结论，无 OEM 文件。
## LuKa 20c94f51c0f0ae88b154c12e4f604643ba23022c 对照
- 上游 start graft hook 0x11ecd0；MU1005 等价指令 0x11ec60，函数入口 0x11e9b0，GOT 字面量确认 BluetoothServices::startBtStack。vtable 0x2be4f8，真实 PT_LOAD 映射后方法槽 0x2be500 = 0x11e9b0。第二 PT_LOAD VA-file offset=0x101000，不是首段 0x100000。
- 0x11ea88 读取 this+0x1b0，非零跳过重复初始化。0x11eab0 -> 0x27b190 -> 0x27a660 -> 0x1e4fc4，0x1e5118 -> 0x243640 Cmgr init，0x243678 -> 0x1db97c 事件订阅；0x11eb00 注册本地 ME handler，0x11ec38/40 启动其余服务。0x11ecdc primary DID 注册，0x11ecf4 成功路径回到 0x11ec60；禁用 DID 或失败也可汇入，故此位置并非独立成功证明。0x11ec98 将 this+0x1b0 置1。0x11dda0 shutdown callback 在 0x11de34 调 0x27b180，0x11de54 清状态；0x125908 清理路径在 0x125968 调同一关闭函数、0x125970 清状态。
- 上游 EIR hook 0x12a710；MU1005 对应 0x12a6a0。0x12a688 r7=this+796；0x12a68c r1=240；0x12a694 调 0x1ddfb4 构建，0x12a698 r1=r7、0x12a69c r2=r0，0x12a6a0 原厂 store length this+0x40c，0x12a6a8 调 0x1dd060 提交。后者验证 <=240 并在 SDK 锁中复制/提交。
- RFCOMM 0x277a18/0x2779b4、SDP 0x207244/0x2071c4 都使用 0x27a12c/0x27a0a0。SDP add/remove 成功路径 0x207340/0x207234 调 EIR refresh 0x1db4c8，refresh 内也有同一锁。锁由 OSAPI singleton+4 提供；递归语义依赖外部 osal::MutexOps::init，不以静态嵌套推断可任意重入。
## 工程接入
现有 upstream EIR 源码与实际 R5 服务状态关联：RFCOMM、iAP2 SDP 和 CarPlay SDP 均已发布，信道与描述符相同，当前 owner、daemon 仍有效，未进入 link/withdraw/dirty。QNX native review 库新增 mu_bt_native_append_eir 入口，复用原 EIR helper，独立有线 idle 回调。真实 EIR 构建 hook 尚未安装，未实车广播；不主动调用 refresh。旧 R5 一次 socket 生命周期仍需原厂回调安全回收后才能重新注册；QNX 上层自动重连通过不代表该原生层已重连。
下一步：锁定启动完成/退出所有者及事件线程实证，接入真正的生命周期；不把静态候选当成可运行补丁。保留原厂 MFi、有线 CarPlay、NO INSTALL。
