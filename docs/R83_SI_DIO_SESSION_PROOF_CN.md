# R83：SI→DIO 跨进程同一会话媒体证明快照（NO INSTALL）

目标：2017 奥迪 A6L C7，MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARMv7。

## R82 留下的关键缺口

R82 只读操作 6 能跨真正的 AF_UNIX 进程读取 DIO 的媒体状态 `0/4/5/7`，但状态码 `7` 无法说明视频、音频、MMI、方向盘事件是否属于 **当前同一原厂 SI/iAP2 会话**。仅凭全局媒体 flag 会误报，不能代表可用的无线 CarPlay。

## 本轮代码

- 新增 CPRX v3 **op11 `MU_CPRX_OP_SESSION_STATUS`**。仅在 DIO 现有授权套接字和原厂接收器已启动、当前会话有效时才执行，不会额外启动/停止 AirPlay。
- 返回格式：CPRA 8 字节头 + `media` (1) + `inner_iap2_mfi` (1) + `channels` (1) + 保留 1 + `generation` (4) + `epoch` (8) + `session` (4)，总计 **28 字节**。所有多字节字段按已有协议使用小端格式。
- 服务端必须已接入 `si_inner_iap2_mfi` 与 `si_media_channel` 两类 **独立的原厂会话证据回调**，任一缺失直接拒绝；任何结果不确定则返回 RETRY，不推断已经成功。
- 视频、音频、MMI、方向盘四个通道对应掩码 `0x01/0x02/0x04/0x08`。仅在 DIO 原生 `media==7` 且内层 iAP2/MFi 属于当前 SI session 时才采样四路通道。`channels==0x0f` 才说明该次查询里的四项证据齐全，仍不表示持续有效。
- SI 端新增 28 字节分段接收和严格验证：服务端回传 `generation/epoch/session` 必须逐字节解析并与原始请求一致；拒绝多余掩码位、无 MFi 却声称有通道、原生媒体不为 7 却声称通道有效、未知状态和保留位错误。BUSY 仍只需 8 字节头，不能卡住等待不存在的载荷。
- 收到身份校验失败、会话所有权失效或非法回复时，清除缓存媒体、内层认证和四路掩码，不能沿用旧会话证据。保持 wired CarPlay 优先与 OEM 恢复屏障不变。

## 运行过的真实跨进程与负向测试

Linux `fork()` 中独立 SI 子进程通过 Unix socket 向模拟 DIO 的父进程请求 op11：无原厂回调→拒绝；状态 `7` 但方向盘未加入当前会话→掩码 `0x07`；四路齐全→掩码 `0x0f`；随后回落状态 `4`→掩码清零；SI 原始 owner 失效→清除缓存。

此外模拟 28 字节**恶意响应报文**，检查错误 generation、epoch、session、媒体状态、内层认证、通道保留位和保留字节全部拒绝。主机 ASan/UBSan、无线控制、蓝牙 iAP2/MFi、双频、CSM 50,000 组和 SD 67/67 回滚测试通过，`cloud12_gate.py` 仍判定 `installable=false`。

## 诚实边界

当前 op11 的 DIO 证明接口 **仍无实车原厂回调绑定**，测试的回调由主机桩实现。协议把需要真实验证的条件显式传过去，**但没有让真实 iPhone 通过 MFi**，没有让 QNX 原厂 SI 获得实际媒体事件，且不能单凭 `0x0f` 说用户可以使用无线 CarPlay。

尚需研究原厂 `smartphone_integrator`、`dio_manager` 的实际导出、同一会话 ID 来源及可靠进程边界；不提供刷写/安装包，不上传原厂二进制、MFi 密钥。状态 `SOURCE_ONLY / NO_INSTALL / vehicle_verified=false`。
