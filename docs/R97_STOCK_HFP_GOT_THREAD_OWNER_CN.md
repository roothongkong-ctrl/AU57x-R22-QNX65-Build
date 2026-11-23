# R97：MU1005 原厂 HFP 回调入口 + 真实线程来源（源码，禁止安装）

基线：2017 奥迪 A6L C7 PA，`MHI2_CN_AU57x_K1006_2/MU1005`，QNX 6.5 ARM32。

## 这轮与 R96 的实际区别

R96 做到了 SD 安装/恢复的主机实验，但没有给原厂 BlueSDK 事件回调找到真实挂点。R97 直接从用户过去上传的**精确原厂** btstack ELF 恢复出其导入的回调指针槽，以及原厂线程创建位置；没有从 MHI2Q 借固件地址。

- 原厂 btstack SHA256：`51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566`。
- `R_ARM_GLOB_DAT HfRfCallback`：原厂 GOT `0x2c23c8`，原函数 `0x2481f0`。原代码从 GOT 装入并保存回调地址的指令：`0x24648c`、`0x2464ac`。原厂 HFP RF 注册点：`0x2464b0`。
- `R_ARM_GLOB_DAT SimClientRfCallback`：GOT `0x2c18c8`，原函数 `0x24c578`。SIM 注册点 `0x24dcac`。这里只分析，不替换 SIM 回调。
- 上述两个 GOT 槽确实落在原厂 ELF 的**可写** PT_LOAD，而回调函数位于原厂可执行区；原厂 ELF 无需重写。
- 原厂四处经真实 ARM 指令验证的 `pthread_create` 调用点：`0x236668/0x2368c8/0x236c2c/0x236c48`，返回地址分别为 +4。

`tools/r97_stock_hfp_got_audit.py` 在私有 ELF 上验证 ELF32/ARM、哈希、GLOB_DAT、可写区以及 5 条关键 ARM 指令。机密原厂 ELF **没有**打入 ZIP 或公开 GitHub。

## 新增原生代码

1. `bt_thread_owner.[ch]`：在 R92 原厂进程 SHA、身份验证后，仅对来自四处已核对的 stock `pthread_create` 调用点包裹真正线程入口，记录本线程身份/原始入口/存活期。线程正常退出、`pthread_exit` 或取消时会清除，16 个槽位耗尽则不阻断原车线程。不能仅靠这个证明 SDK 锁或回调拥有权。
2. `bt_got_observer.[ch]`：**实验开关** `AU57X_R97_GOT_PROBE=1`，默认关闭。在原厂进程中、哈希符合且尚未注册 HFP 时，以原子指针比较交换，将 HFP GOT 的原函数指针换为小型透传观察器，读取事件第一个字节、统计是否来自已观察的本地线程，然后始终调用原厂 HFP 回调。失败/不匹配时不修改。只观察 HFP，不接管 HFP 的音频、不启动新的 RF 服务、不注册 CarPlay。
3. `tools/r94_launcher_patch.py --experimental-hfp-observer`：在原本可逆的**离线模拟配置**中添加显式环境变量，不使用时完全保持 R94 默认行为；保留原厂的其他配置、preCondition 及启动参数。
4. `ci/host_r97_continuous.py`：连续执行 9 组独立回归，有失败也继续其他组；日志和具体未验证事项写到 JSON。

## 验证边界与特别限制

- 新的 GOT 观察器已经在主机实际用函数指针转发五种模拟回调事件；真正的 MU1005 **HFP 蓝牙无线电回调**尚未在原车执行；不可以称为实际 RF 连通。
- **HFP 不是 iAP2**。即使以后拿到原车 HFP 事件线程，仍需独立 iAP2 RFCOMM 注册、同次手机 MAC 身份、实际 Apple MFi 回执和 2.4GHz Wi-Fi/AirPlay 才能接通无线 CarPlay。
- GOT 被原厂设备对象缓存以后，单纯还原 GOT 可能无法撤销已经注册的函数指针，所以不能在原车运行期间随意卸载 preload。任何安装与恢复必须由原厂监督者完成完全注销、回调排空和可靠启动恢复；目前没有证明。
- **不能从当前 R97 产生一个安全的车机 SD 安装器**，R96 原主机实验仍是独立可用的源码资产。`installable=false`、`vehicle_verified=false`、`safe_to_flash=false`。
- 用户要求在全部电脑端整合前不上车，因此本轮没有请求车辆日志、SSH、OBD 或文件。

## 运行离线验证

```bash
bash ci/host_r97_owner_thread.sh
bash ci/host_r97_hfp_got.sh
python3 -m unittest discover -s tests -p test_r97_stock_hfp_got_audit.py -v
python3 ci/host_r97_continuous.py
# 原厂文件仅在用户自己保留的私有目录运行：
python3 tools/r97_stock_hfp_got_audit.py --btstack /private/btstack --out /tmp/r97_oem_got.json
```
