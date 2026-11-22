# R81：把原厂 SI 客户端与 DIO 接收器分开进程（NO INSTALL）

目标固件：MHI2_CN_AU57x_K1006_2 / MU1005，QNX 6.5 ARMv7。

## 依据与需要纠正的假设

已获取的 MU1005 原厂 ELF 静态核查记录（`V32_MU1005_audit.json`，仅用于本地分析）显示：

- `smartphone_integrator` 是单独的 ARM32 ELF 可执行程序，直接依赖 `libirc_mmx_adapter.so`、`libcomm.so` 等；其直接 DT_NEEDED 清单**没有** `libairplay.so`。
- `dio_manager` 也是单独的 ARM32 ELF 可执行程序，直接依赖 `libairplay.so` 和 `libiap2client.so.1`，并具有大量原厂 AirPlay 会话回调符号。
- 这不能证明运行时一定没有特殊的动态装载行为，但已经足以否定“两个程序可直接共享同一个进程指针”是已被验证事实。R78–R80 把 `MuCprxService*` 直接传入 SI 控制客户端的方式只能用于同进程研究，不是实车已经接通。

## 本轮实际改动

在 `cprx_service.h/.c` 新增 `MuCprxRemoteEndpoint` 和 `mu_cprx_client_begin_remote()`：

1. SI 客户端只保存一个独立的端点快照（Unix socket 绝对路径、generation、epoch、session、AirPlay 端口、原厂 DIO 的预期 UID/GID），不解引用另一进程中的 `MuCprxService` 或 DIO 指针。
2. 建立实际 `AF_UNIX SOCK_STREAM` 通道，发送 32 字节 CPRX v3 START 或 STOP，非阻塞分段读写真实服务端响应，复用已有协议和超时处理。
3. 发起前验证目录 owner 和 `0700` 权限、socket owner 和 `0600` 权限；连接后核对系统返回的 DIO UID/GID。
4. **强制要求** `si_owner_verified` 和 `dio_server_verified` 两个独立回调。没有任意一个回调，或任意一次返回失败，都拒绝启动/停止；有连接方证据但在发包前被撤销时，仍拒绝写入。
5. 经过真实 Linux `fork()` 测试：父进程模拟原厂 DIO 接收器，子进程仅通过独立 socket 客户端执行 START→ACK→STOP→ACK；客户端不会访问父进程的 service 地址；错误 UID、失效 SI session、缺少回调、连接后撤销 SI/DIO 权限均拒绝。父进程准确收到 1 次 START、1 次 STOP，恢复并删除 socket。
6. 已有 R80 同进程接口暂时保留供离线比较，不再把它当作实际车机接线的充分证据。QNX 交叉编译与设备运行是两个不同的验证关口。

## 仍然阻塞（必须保留）

- **尚无生产实现的** `si_owner_verified` / `dio_server_verified`：需用真实 `smartphone_integrator`、`dio_manager` 各自进程内可证实的 owner / session / 进程身份事件完成，不能从 wire token 或 root UID 推断。
- 未核实车机上两进程的实际 UID、GID、socket 私有目录位置与权限；若不是同 UID/可授权访问，需在 OEM 原有 IPC 边界实现，而非简单更改权限。
- 未接通原厂 Apple iAP2/MFi 无线握手、真实 iPhone AirPlay 视频音频/旋钮/方向盘事件；这份代码不具备直接装车条件。

状态：`SOURCE_ONLY / NO_INSTALL`、`safe_to_flash=false`、`vehicle_verified=false`。固定 12 步账本状态不调整，不包含原厂 ELF/固件/密钥。
