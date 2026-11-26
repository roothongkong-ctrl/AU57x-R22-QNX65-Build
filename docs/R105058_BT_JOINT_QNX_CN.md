# R105.058 — 第01步：QNX ARM 联合运行

R105.057 主机测试 45912098593 的实际日志已出现 R105057_JOINT_OWNER_RFCOMM_TWO_SESSIONS_PASS_FIXTURE_ONLY_NO_INSTALL。此次新增专用 CI，复用已固定的 QNX 6.5 ARMv7 工具链、QEMU、socket manager IFS 准备代码；仅执行 R105.057 联合用例，不重跑旧 QNX 全链路夹具。实际编译本次分支中的测试和蓝牙协议代码，检查真实 QNX 进程退出 0 与完整 PASS 标记。旧 ARM payload 仅作为既有 IFS 构建输入，不作为本轮联合测试执行证据。

仍以显式夹具替代原厂启动、BlueSDK 调用及手机事件。原厂生产启动调用者、真实生命周期 lease 和手机连接仍无运行证据。禁止把编译或夹具通过作为原厂注册成功。NO_INSTALL。
