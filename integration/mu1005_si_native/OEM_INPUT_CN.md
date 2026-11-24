# 原厂回调定位：一次性离线取证（NO INSTALL）

当前 ARM 库已通过云端任务 45432872625。真正的阻塞点是原厂 SI/DIO 回调绑定，而非再次编写模拟回调。R86 已证明 delegate 的 60/56 字节布局及 21 个回调槽，不证明实际 MFi 认证事件。

在 Mac 或电脑版 AI 上，将已提取的 MU1005 app.img 文件树作为 root，不要上传原始固件到公开 GitHub：

```sh
python3 tools/mu1005_oem_inventory.py "/path/to/extracted-app" --out mu1005-oem-evidence.json
```

只读查找三个 SHA256 已固定的 ELF：smartphone_integrator、dio_manager、libairplay.so；同时记录 libiap2client.so.1、libirc_mmx_adapter.so、iap2.cfg（存在时）。只报告相对路径、哈希、依赖与候选符号；不导出配置内容或私钥，不改动原文件。文件名不同或原始 img 未解包时需先提取/重命名副本。候选符号仅为后续反汇编线索，不作为原厂回调已绑定的证据。

如果原厂文件仍在私有工作区，优先复用；若不在，请一次性提供上述三个匹配哈希的原厂 ELF，辅助文件尽量同包提供。原厂程序不提交到 GitHub。没有 OEM 真实认证事件证据时不声明功能实现。保留有线 CarPlay，不绕过 MFi。
