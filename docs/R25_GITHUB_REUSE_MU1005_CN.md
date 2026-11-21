# AU57x / MU1005 无线 CarPlay：GitHub 源码复用排查（R25）

日期：2026-11-22。目标：2017 Audi A6L C7，`MHI2_CN_AU57x_K1006_2 / MU1005`，QNX 6.5 ARMv7。**本报告只用于研发，禁止直接装车**。

## 一、已经找到的可以复用的实质部分

1. **优先：`luka-dev/mib2q-carplay-rgi` 的 `wireless/graft`、`wireless/daemon` 和 `wireless/si`。** 有真实 C/Rust 源码及 QNX 测试，并非纯文章。适用的**机制**包括蓝牙 RFCOMM 事件转发与线程隔离、iAP2 链帧/CSM、MFi 授权接口隔离、SI 单独持有 DIO 子进程生命周期；**不能直接复用固件地址、EIR/SDP 注入指令和已编译的 MHI2Q 产物**。特别是 `graft.c` 的地址属于 MU1316／其他绑定的 MHI2Q 哈希，与我们的 MU1005 不同。库有 **GPL-3.0**：复制/派生源码到公开仓库必须处理许可、归属与对应源码义务。
2. **直接复用我们的旧 `R14_FINAL.zip` 更合适。** 在 `legacy/source/bluetooth/` 里已经有 `native_binding.c`、`native_event.c`、`inbound.c`、`inbound_r5.c`、`native_binding_words.h`、RF/IP C/socket 子模块。它们与 Luka 路线相同的部分：RF channel `0xB0=176` 字节；TX packet `0x64=100` 字节；SDP record `0x24=36` 字节；`MAX_DATA=5035`；callback `+0x08`、MTU `+0x0c`、context `+0x14`；事件 `1/2/4/5/6`；有 8 字节 `type|reserved|length|generation` 帧头。源码布局一致**不等于两台机器的 OEM 内部调用地址相同**。
3. **新的独立验证成果：** 用户保存的 R14 原生地址指纹表 `native_binding_words.h` 有 **40** 条，每条四字节。用当前保存的原厂 `btstack`（SHA-256 `51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566`）的可执行 PT_LOAD 映射逐项比较，**40/40 一致**。说明 R14 的这批 MU1005 指令地址确实与**该原厂样本**一致。它还**不能证明** iAP2 蓝牙端点的发布者、线程锁、对象生命周期、配对身份、现场固件 SHA 或启动时的回调归属。检测器：`verify_mu1005_btstack_words.py`；完整本地结果：`MU1005_BTSTACK_WORDS_AUDIT.json`。不要把原厂二进制上传到公开 GitHub。

## 二、新调查的 GitHub 项目与复用判定

| GitHub 项目 | 已核实提供的内容 | 针对 AU57x MU1005 的判定 |
|---|---|---|
| [luka-dev/mib2q-carplay-rgi](https://github.com/luka-dev/mib2q-carplay-rgi) | `wireless/graft/graft.c`、`test_rfcomm_peer.py`；`wireless/daemon/vendor/iap2-link` / `iap2-csm`，`wireless/si`、接收器生命周期源码 | **优先复用算法及测试思路**；QNX/ARM ABI 接近，MHI2Q 内部地址不可复用；GPL-3.0 |
| [congobg/MHI2Q_WiFi_CarPlay](https://github.com/congobg/MHI2Q_WiFi_CarPlay) | MHI2Q 3638/5092 的无线 CarPlay 实车工程过程，证明其方案由 btstack hook、iAP2 daemon、原厂 AirPlay 和 HMI 组合而成 | **架构参考优先**，但已检查该公开 repo 仅约 15 个文件，核心实现源码没有直接提供；不能宣称可直接下载使用，无明确仓库许可证 |
| [omonob/VW-MHI2-Wireless-Carplay-Maps](https://github.com/omonob/VW-MHI2-Wireless-Carplay-Maps) | MU1102 / MU1367 等版本的配置、安装、恢复、版本校验及事务脚本 | **第 12 步可借鉴备份/回滚/安装前精确签名拒绝机制**。未适配 MU1005；不复制二进制或固件配置；GPL-3.0 |
| [joeyQuery/MHI2-wirelessCarPlay](https://github.com/joeyQuery/MHI2-wirelessCarPlay) | MHI2 MU0678 的 iAP2 资源设备、`CIapBTChannel`/`IapDeviceServices` 静态研究 | **有助于对照路径，但不能直接复制**。精确 MU1005 的 `btstack` 中尚未定位其 `/dev/iapDevice`/`IapDeviceServices` 路径；该库尚未证明完整无线功能，未明确许可 |
| [lvalen91/CPC200-CCPA_resources](https://github.com/lvalen91/CPC200-CCPA_resources) | MIT 协议研究，iAP2 蓝牙引导与 MFi/Identify、`0x5702/0x5703` Wi-Fi 消息顺序（有独立适配器抓包来源） | **可复用消息编号、合法状态流及测试用例设计**；适配器的 `/dev/rfcomm0` 和协议角色不能直接映射到奥迪；MIT |
| [lvalen91/carplayd](https://github.com/lvalen91/carplayd) | Rust `crates/iap2-core` 链帧/状态/消息；无线模块 | **适合生成离线对照协议测试**，但采用 Linux/外置接收器架构，不能把 Rust 系统服务直接塞进 QNX；Unlicense（仍核对各依赖许可证） |
| [oguzkarcan/EntryNav2-CarPlay](https://github.com/oguzkarcan/EntryNav2-CarPlay) | 开源 C++11 BT transport、SDP、mDNS、独立接口封装与测试 | **可复用通用协议设计/测试思路**；BMW ALPS/BlueZ、Linux 3.12 与奥迪 QNX BlueSDK 不同；GPL-3.0 |
| [edzenis/mib2high-carplay-altscreen-rgi](https://github.com/edzenis/mib2high-carplay-altscreen-rgi) | 非 MHI2Q 的 MHI2 RGI/HMI 源码 | **第 09 步 UI/HMI 复用候选**，非无线 RFCOMM 实现；需核对平台与许可 |
| [yuedizhibo/MHI2Q-CarPlay-AltScreen](https://github.com/yuedizhibo/MHI2Q-CarPlay-AltScreen) | MHI2Q 第二屏、RGI/电话与仪表显示 | **本项目的仪表支线参考**，不是 MU1005 原生无线 CarPlay 软件；其说明明确不支持普通 MHI2 |

## 三、第 05 步正确的下一段研发（禁止重复空转）

已不需要再开发一套新的 RF 队列，也不需要重复证明 16/18/19 C 文件可以编译。优先取用户旧工程保存的 **R14 原始 `native_binding.c / native_event.c / inbound*.c`** 与已经通过 QNX 编译的 R24 模块整合，保留原文件的**原厂生命周期核验、真实配对身份、异步发送缓存、失败回滚**规则。首先只在离线方式完成一致性验证，不发布“车机可安装”的 hook。

验收第 05 步还需补足：

1. **原厂 BlueSDK owner 能否合法调用真实注册/回调**：需证明 owner 的启动阶段、独占/锁和回调是否实际交给我们的模块；40/40 指令指纹并不证明执行时序。
2. **真蓝牙数据的去向**：确认数据来源是原车实际连上的 iPhone 并能够到达经过验证的 iAP2 端点；自制 QEMU 假帧不算。
3. **一次蓝牙 iAP2 正常状态链**：连接→检测→Identification→MFi 认证由原厂授权结果驱动，不伪造任何认证成功。

**正式进度仍是第 04 步完成、第 05 步未完成**。第 13 步实车验证仍保留最后，但纯云端无法证明真实手机和原厂硬件之间的无线链路已经工作。

## 四、可复验命令（仅本地原厂文件，禁止上传公开仓库）

```sh
python3 verify_mu1005_btstack_words.py \
  --btstack /path/to/private/btstack \
  --legacy-zip /path/to/R14_FINAL.zip \
  --output /path/to/MU1005_BTSTACK_WORDS_AUDIT.json
```

仓库中只提交脚本、分析文档与**静态核验结果**，不提交奥迪原厂二进制、身份数据或无许可证的整段外部源码。