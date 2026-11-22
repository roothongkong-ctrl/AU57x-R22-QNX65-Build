# R86 原厂 MU1005 AirPlay 两级回调注册的实际 ABI（只读研究）

- 适用：2017 Audi A6L C7；`MHI2_CN_AU57x_K1006_2 / MU1005`，QNX6.5 ARMv7。
- 输入：用户早先提供的原厂 `dio_manager`、`libairplay.so`、`smartphone_integrator`，按 SHA-256 固定为唯一可信固件快照。
- 状态：**`NO INSTALL` / `safe_to_flash=false` / `vehicle_verified=false`**。没有修改或启动车机进程。

## 这次真正定位到了什么

**服务器注册：** 原厂 `dio_manager` 在 ARM 虚拟地址 `0x15DF68` 仅一次调用 `AirPlayReceiverServerSetDelegate`（PLT `0x112514`）。其调用前向 `r1` 提供本栈 `sp+0x4C` 的回调结构体，调用前将 60 字节置零，将原厂 DIO 本地管理器上下文 `[r5+8]` 写到首字段，`r0` 是 DIO 本地 `[r5+12]` 保存的 AirPlay 服务器引用。原厂 `libairplay.so` 在 `0x1CEEC` 实现了该函数，确实把 **60 字节**从 `r1` 复制到服务器内部偏移 `+16`，并非仅保存一个回调指针。

**会话注册：** `CDIOManager::s_sessionCreatedCallback`（`0x159374`）中在 `0x159448` 调用 `AirPlayReceiverSessionSetDelegate`（PLT `0x11270C`）。收到新的 session 引用（回调形参 `r1`）后，它清零本栈 **56 字节**，保留 `r2` 上下文至偏移 0，然后用 `r0=session`、`r1=sp` 设置会话回调。原厂 `libairplay.so` 在 `0x1FB44` 确实将 **56 字节**复制到 session 内部偏移 `+20`。

两级回调的每个非空槽都由真正的 `R_ARM_GLOB_DAT` 重定位指向原厂 `CDIOManager` 符号，且 PIC GOT 地址从原始 ARM 机器码重新算出。详细 21 个槽位、ARM 地址、GOT 地址和完整 SHA-256 见 `R86_OEM_DELEGATE_AUDIT.json`。

**服务器回调（11 个）：** serverCopyProperty、sessionCreated、sessionCreationFailed、sessionScreenCreated、audioType、waitAudioAvailableAck、tx/rxAudioDumping、tx/rxAudioFlush、initAudioDumping。

**会话回调（10 个）：** sessionFinalized、handleSessionControl、sessionCopyProperty、sessionNotifyProperty、sessionModesChanged、sessionRequestUI、duckAudio、unduckAudio、audioTornDown、airplayError。

## 与 R85 的区别与下一步

R85 里 `si_inner_iap2_mfi`、原车视频/音频/控制证据和 `dio_wired_takeover_verified` 必须由真正进程提供。R86 首次从你已有的原厂机器码证明了 DIO 注册的两级回调结构、准确长度、固定入口和真实符号重定位，提供建立只读进程内观察层的事实基础，而非继续凭测试桩猜 ABI。

下一关键环节：要在合法、单拥有者 DIO 生命周期内实现**不改变原厂回调执行与返回值**的事件观察，按原始 session 身份将 `sessionCreated/sessionFinalized` 与后续媒体消息关联；不能把“创建回调触发”直接等同于 MFi 成功、实际显示、物理有线证据。`smartphone_integrator` 与 `dio_manager` 是不同进程，**不可用 SI 直接引用 DIO 回调或服务器指针**。必须另外实现绑定、权限检查、媒体认证和回滚。只读回调定位本身不提供安装入口。

`tools/mu1005_oem_delegate_audit.py` 完全使用 Python 标准库解析 ELF32 机器码、动态符号表、重定位和 PIC 地址，先严格校验原厂 SHA-256，然后验证两级注册、长度和所有 21 个 GOT 回调。不带原厂二进制，也不能用于刷机。

离线运行（自行提供原版三个文件）：

```bash
python3 tools/mu1005_oem_delegate_audit.py \
  --dio /path/to/dio_manager \
  --airplay /path/to/libairplay.so \
  --si /path/to/smartphone_integrator \
  --out R86_OEM_DELEGATE_AUDIT.json
```

公开 CI 运行纯合成负面测试；本地私有原厂件可以通过 `MU1005_DIO`、`MU1005_AIRPLAY`、`MU1005_SI` 环境变量额外运行完整对照与篡改拒绝测试。保留原厂文件在私有路径，**不提交或打包它们**。
