# R93 — 第二步第一项：原厂 btstack 动态入口证明与一键 SD 取证

固件：`MHI2_CN_AU57x_K1006_2 / MU1005`，原厂私有 `btstack` SHA-256 `51ecbd44c3581bd69f01ed5bdc053ba28ccc6fef3bf1bd4c3a8f010898ab0566`。

## 此次确认的真实部分（不是模拟）

- 在**用户原始 QNX ARMv7 btstack** 中验证 ELF interpreter `/usr/lib/ldqnx.so.2`、导入库 `libc.so.3`。
- 直接解析原始 ARM `BL` 指令：`0x236668`、`0x2368c8`、`0x236c2c`、`0x236c48` 全部指向 PLT `0x117390`；GOT `0x2c148c` 存在 `R_ARM_JUMP_SLOT pthread_create`。这从结构上证明 QNX LD_PRELOAD `pthread_create` 插桩的入口存在。
- 修正 R92 的进程身份误判：旧 R92 只哈希原厂硬盘上的 btstack，无论加载器实际被哪个进程装入都可能记录 `stock-image-pinned`。R93 首先用 QNX 6.5 正式提供的 `__progname` 校验真实进程 basename 必须为 `btstack`，否则记录 `wrong-process` 并禁用跟踪。
- 原来动态 `LD_PRELOAD`、哈希门禁、线程钩子、BlueSDK 后续适配、iAP2/MFi 认证继续保留。

## 一次 SD 取证操作（不需要 SSH；不会触碰原厂系统）

1. 用现有 **M.I.B. SD1 卡**，**先备份**你卡里的 `/mod/custom.sh`（若本来就有），把本包 `R93_SD_MIB_ROOT/mod/custom.sh` 复制为 SD 根目录的 `/mod/custom.sh`。不要把整个 R93 ZIP 当固件刷。
2. 汽车启动 MMI，SD 插入 **SD1**；绿屏进入 **M.I.B. → Advanced Settings → Run Custom Script**，执行该项一次。
3. 取出 SD，在 SD 根目录得到 `R93_BTSTACK_PROOF/diag.txt`、`status.txt`，以及如有的 `connectivity_json_copy.txt`、`loader.log`；把整个文件夹提供给研发。

脚本只读取：当前 `btstack` PID、`pidin mem` 动态库列表、原厂 ELF 的存在及可选 SHA256、可见的 `connectivity.json` 和以前 R92 的探针日志。它**不**停止/重启 btstack，不设置 LD_PRELOAD，不修改原厂固件。它可以确定当前进程的加载状况及下一步修改启动参数所需的配置文件路径。仅凭日志文件存在不能确认加载成功；必须匹配当前 PID 和其动态库映射。

**当前状态：原厂 ELF 可重定位调用结构=已证明；原车启动进程实际加载 R92.so=未证明。** 需要原车一次部署/运行记录才能完成第 2 步第 1 项的最后验收。没有 SSH 不妨碍 SD 取证，但取证不等于注入。

## 工程与回归

`bash ci/host_bt_preload_probe.sh`

`bash ci/host_r93_plt_audit.sh`

使用私有原厂文件进行额外静态验收：`R93_PRIVATE_BTSTACK=/private/MU1005/btstack bash ci/host_r93_plt_audit.sh`。

GitHub 公共工程和 ZIP 不含原厂固件二进制。R93 源码版并非无线 CarPlay 实车安装包。
