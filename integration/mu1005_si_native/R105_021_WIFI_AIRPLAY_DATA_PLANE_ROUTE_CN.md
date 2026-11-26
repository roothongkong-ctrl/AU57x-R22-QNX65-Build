# R105.021｜纠正旧阻塞状态并切换到 Wi-Fi / AirPlay 数据面

日期：2026-11-26
目标：MHI2_CN_AU57x_K1006_2 / MU1005 / QNX 6.5 ARM32
基线：R105.020 + 第 25/27 轮原始交接 + 当前分支 inner_iap2 与 AirPlay 源码
状态：ROUTE_CORRECTION / SOURCE_REVIEW / NO_INSTALL

## 1. 本轮纠正过期阻塞，避免重复索取文件

旧的第 22 轮报告曾把以下依赖记为 MISSING：libcomm.so、libutil.so、libosal.so、libiplcommon.so。

但第 25 轮交接已明确提供并 SHA256 校验六个 ImageFS 原厂库，QNX 6.5 装载器逐项接受：
- libosal.so：520,192 B，SHA256 20345734f469d8a09bb0e8e3ca2d7b4cecfa915308353ee91d230a37c104de11
- libutil.so：950,272 B，SHA256 589d575060d76b7c71c2f964e5f94e9e5c8d35be1fb13f7ab3e7eb7e627a69f0
- libcomm.so：823,296 B，SHA256 2eb1d8550c3ad4a8cbdafba567168189dc2cb195d1f4b12b0661afe1a19b12a8
- libiplcommon.so：299,008 B，SHA256 3a8b53926b1605adf52325334aed7790c1fd9ae2d027011d5950e88d9bddefaf
- libdsicommon.so：28,672 B，SHA256 a76376640eb28b6a8e10d7ec350affbb243ba5addeb01883208f5dac04fe381a
- libservmngt.so：131,072 B，SHA256 14dd5264ced3dc033759761bc2db694d5f3ea815b229f4a622d86d9a6ca52c6b

第 25 轮真实 QNX loader 结果：这六个库已被接受；smartphone_integrator 下一项缺 libirc_mmx_adapter.so；btstack 当时下一项缺 libicuuc.so.58；iAP proxy 的依赖闭包为 0 缺失。第 26/27 轮随后完成 QNX6fs 按名提取，并明确修正第 25 轮关于 libirc 不在 app.img 的旧判断：libirc_mmx_adapter.so 已从 ImageFS 取出，296,740 B，SHA256 3277e0c4…ba9b2。

因此：不得再把这四个库列为当前缺失项，也不得重新向用户索取已交付的库或相同原厂样本。仍须对实际用于每个进程的精确文件版本/哈希做依赖闭包核验，不能把旧轮次的装载结果自动当作当前分支全链路运行成功。

## 2. ASI iAP proxy 裸数据路线已由第 27 轮终止

第 27 轮在 app.img 的 778 个 ARM ELF 中检索确认：
- asi.connectivity.bluetooth.iap.IapDeviceServices 只存在于 libasimmxconnectivity_bluetooth_iapproxy.so 和 EMS 契约表；
- 找不到该接口的服务端实现；
- 9 个方法 ID 0–8 是 reconnect、active device、local address/name、device list 与字符串控制消息；
- 没有 byte[] / stream / frame / payload 形式的方法；
- stringRequest/stringResponse 仅 string，且无服务端实现。

所以本分支不再投入 ASI IapDeviceServices 裸 iAP2 字节桥，不把 libcomm 等运行库补齐误当成数据面突破。ASI 仍可承担已有控制面，但不是 iAP2 原始帧传输。

## 3. 当前已有可复用的 AirPlay / iAP2 数据面代码

现有源码已具备以下可复用模块：
- inner_iap2.c：FF55 detect、FF5A 长度/校验和帧解析、原包转发、epoch fencing、部分写与背压状态。
- inner_socket.c：只接受已验证的 AF_UNIX SOCK_STREAM、非阻塞、CLOEXEC、peer proof；断线关闭 relay。
- inner_airplay.c：在已证明的原厂 AirPlay library/session owner 下解析 iAPSendMessage 的 {type, params:{data:CFData}} 命令并调用原厂 AirPlayReceiverSessionSendCommand；非相关命令返回 2 交还原厂，错误不伪造 ACK。
- OEM_AIRPLAY_ABI_CN.md：记录 libairplay.so 的 AirPlayReceiverSessionSendCommand 及 CFDictionary/CFData 相关静态 ABI。
- cprx_service.c：DIO receiver 生命周期、身份、SI owner、Unix peer 与恢复屏障逻辑。

这些源码仍是适配层/离线实现。test_airplay.c 明确使用 host-only fake CFL library；它只验证适配器行为，不证明 OEM ABI、原厂运行时 owner 或真实 iPhone 会话。

## 4. 正确的下一条数据链

目标数据链（待完成，不是已接通的运行证据）：

iPhone
  → Bluetooth discovery / pairing（control/bootstrap）
  → OEM Bluetooth + DIO / SI control state（配对身份与模式状态，不承载 iAP2 raw frames）
  → OEM Wi-Fi network / AirPlay receiver
  → real AirPlay session + event/control callback owner（尚未 runtime-proven）
  → AirPlay iAPSendMessage { params: { data: CFData } }
  → inner_airplay adapter
  → inner_iap2 packet relay（FF55 / FF5A；保留原始字节）
  → verified AF_UNIX IPC with generation/epoch/session
  → OEM AirPlay / iAP2 receiver path

原车 Wi-Fi AP 的历史连接只证明基础 IP 连通，不证明 iAP2-over-network、AirPlay 或 MFi 成功。

## 5. 下一轮可执行工作（不重复已完成的 ABI/CI）

1. 从 dio_manager 对 libairplay.so 的实际导入与调用点继续追踪：定位 AirPlay server/session 创建、event client 注册、控制命令分派与 session teardown 的真实 owner。
2. 对照 AirPlayReceiverSessionSendCommand 的已知四参数 ABI，找出接收端 iAPSendMessage event callback 的注册点、参数生命周期、回调线程及返回语义。
3. 找到原厂 Wi-Fi 接口/IP/端口由谁配置与发布；从启动脚本、服务配置和 DNS-SD/mDNS 注册中确认真实 AirPlay receiver 对外端点。不得硬编码历史 IP。
4. 把 inner_airplay 的 host fake 测试与原厂 ABI 静态证据分开；增加仅在真实 owner proof 成立时允许 bind 的适配边界，不增加伪造 OEM callback 的 fixture 通过条件。
5. 继续复用已通过的 QNX ARM 编译链，构建并静态核验 AirPlay inner adapter 与 socket relay 的目标对象；QNX 运行/实车仍单独标记 NOT_TESTED。
6. 若缺少实际 event callback owner、AirPlay session 对象或运行端点，明确列出精确缺失的二进制/符号/配置路径，不再转回 ASI raw bridge，也不反复跑已通过的编译/ABI检查。

## 6. 本轮对云端 #96 的复核

GitHub Actions run 45800312869（统一开发）已完成 SUCCESS：
- native-library：job SUCCESS；包含锁定 QNX ARM toolchain 构建和 SI/CPRX transport QNX ARM 编译/链接。
- 6 个 parallel-host lane：均 SUCCESS。
- upstream-graft：7/7 SUCCESS。
- collect-review：成功收集 review-only artifacts。
- mu1005-si-cprx-arm-NO-INSTALL artifact：431,067 bytes。
- MU1005-unified-review-NO-INSTALL artifact：453,917 bytes。

该工作流的一个 cache-save 步骤为 skipped，不影响 job SUCCESS。产物名显式标记 NO-INSTALL；工作流写明 no OEM execution or vehicle verification。不能称为原厂 AirPlay 已运行、无线 CarPlay 已连接或可刷写。

## 严格结论

已纠正过期依赖清单；已确认 ASI iAP proxy 不提供原始 iAP2 帧通道，停止该路线；下一研发重点为原厂 AirPlay session/event callback 与 Wi-Fi 数据面。

当前仍未证明：
- 原厂 AirPlay session owner / event callback 的运行时绑定；
- iAPSendMessage 在本车实际 AirPlay event/control 链中的接收回调；
- 原厂 Wi-Fi AirPlay 端点与 iAP2-over-network 的真实协商；
- iPhone MFi、音视频/输入闭环及实车稳定性。

installable=false / safe_to_flash=false / vehicle_verified=false / mfi_apple_verified=false。
