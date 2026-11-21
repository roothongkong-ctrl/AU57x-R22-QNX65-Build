# 开源来源与许可提示

本仓库中的研究源码由不同历史版本的用户研发代码与开源协议参考整合而来。部分 C 文件明确标注 **GPL-3.0-only**，本仓库的发行许可证为 GPL-3.0。该许可证不能消除第三方代码本身的额外归属和上游出处义务。

- [shihabal3amri/DiPlay](https://github.com/shihabal3amri/DiPlay)：研究中的 iAP2 CSM 控制消息编码/状态处理，现有 `cp_csm.c`、`cp_wireless.c` 等文件已有 DiPlay 派生标注。应保留其原著作权与 GPL 许可。
- [luka-dev/mib2q-carplay-rgi](https://github.com/luka-dev/mib2q-carplay-rgi)：MHI2Q 无线 CarPlay 结构、RFCOMM 和 QNX 协议机制对照，GPL-3.0；**没有**将适用于其他固件的原厂地址和编译二进制当作 MU1005 兼容证据。
- [lvalen91/CPC200-CCPA_resources](https://github.com/lvalen91/CPC200-CCPA_resources)：协议消息和状态序列参考，MIT，不能把适配器设备路径视为奥迪真实端点。
- 用户保存的历史 AU57x R14–R31 资料：固件相关原创适配与静态分析记录。原厂 ELF 不在公开仓库，也未获得用户以外的第三方再分发授权。

公开的研究成果不包含 Apple 密钥、配对机密、专有的 QNX SDK、奥迪 OEM ELF 或已经可刷写的安装包。第三方授权不能以“研究测试通过”代替原厂实车验证。

- R43: `project/source/src/cp_step04_link.[ch]` is a bounded C99 adaptation of Luka iAP2 link sans-I/O code at https://github.com/luka-dev/mib2q-carplay-rgi/blob/main/wireless/daemon/vendor/iap2-link/src/lib.rs (GPL-3.0). Changes: C99 fixed-size buffers, AU57x generation/owner guards, single outstanding CONTROL message, normalized RFCOMM queue; not a complete port.

- R45：`project/source/legacy_mu1005/bluetooth/{native_binding,native_event,inbound,inbound_r5}.[ch]` 直接来自用户此前提供的 `R14_FINAL.zip` 历史AU57x工程，保留R14原始实现及安全门禁；蓝牙OEM不在公开仓库，固件地址仅限静态哈希绑定样本。

- R50 `tools/review_txn/` 来源：用户先前提供的 AU57x_Wireless_Consolidated.zip 中 `source/install_native/`，本次恢复并纳入云端测试，无厂商固件、安装文件清单或真实OEM执行参数。
