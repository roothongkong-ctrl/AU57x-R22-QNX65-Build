# R95：一次连续推进第 3～17 步（2026-11-22）

## 结论（不能混淆）

已**实际连续执行 15 项离线验收**，对应前序计划的小步骤 3～17：15/15 的*主机契约测试*通过；其中若干环节使用模拟 OEM 回调、模拟 MFi 证书和 localhost TCP。**这不代表 15 个生产步骤都完成**，也不证明实际 iPhone 已连接。保留真实硬件欠缺项，最后一次实车验证再确认。

本轮是接续 R94 的真实源码修改与验收，不重写已有已通过的 R45/R44/R87/R85 模块。

## 实质新增

1. **R94 QNX 编译失败原因已定位**：GitHub Actions run `45038542029` 中 Docker 建置阶段，GCC 8.5.0 原始 tarball 的 `curl` 下载报 `exit code 18`（HTTP 内容未完整传输），并非已经看到新增 C 源代码的编译报错。`tools/r95_toolchain_retry.py` 对公开 `luka-dev/qnx65-armv7-toolchain` 的精确两条 GNU GCC/binutils tarball 下载命令添加有限重试、HTTP/1.1，**保留原来的 SHA256 校验**；上游格式变化时拒绝修改。4 个正反单元测试通过。真实 QNX CI 另行核验。
2. **SD 事务从固定试验夹具提升为可生成的、哈希和权限绑定的 C 清单**：`tools/r95_sd_plan_codegen.py` 在有标记的专用主机测试目录中检查成对旧/新文件、SHA256、文件名、硬链接/软链接、目录所有权、模式和备份/状态目录空置，生成 C `txn_spec` 绑定源代码；由已有 QNX/POSIX `tools/review_txn/txn.c` 真实执行事务。
3. **实际编译 C 程序进行恢复验收**：生成的 C 程序与既有 `txn.c`、`sha256.c` 结合编译，执行 `trial`→校验 `PENDING_HEALTH`→校验目标变更和备份→`recover`→校验 `RESTORED` 和原文件全部恢复；正式健康检查未接入，不开放 `confirm` 命令。新增 11 个边界/负面测试均通过。
4. **第 3～17 步连续运行器**：`tools/r95_stage_review.py` 一次运行已有 15 组主机验收，失败/阻塞会记录后继续下一组，并输出 `r95_result/R95_STEPS_03_TO_17_EVIDENCE.json`。每个步骤均显式记录缺少的真实 OEM/手机/无线证明，并将 `vehicle_complete=false`。
5. 原有 40 个 C 源文件和 67 个 C 事务恢复用例继续保留。新增离线报告、CI 回归和开源源码 ZIP；无私有原厂二进制或苹果凭据。

## 逐步判断：第 3～17 步

| 步骤 | 已完成的离线工作 | 尚不能宣称完成的实车端工作 |
|---|---|---|
| 3 | 原厂启动器 ELF 静态审计、启动配置改写/备份模板通过测试 | 尚无用户原车精确 connectivity.json 与真实 supervisor 继承证据 |
| 4 | LD_PRELOAD 动态加载、线程创建原厂 PLT 静态验证 | 未绑定原厂 BlueSDK RF 事件线程 |
| 5 | 原厂 RF_RegisterServerChannel 静态调用点和实验参数代码 | iAP2 第三参数/真实原厂注册未验证 |
| 6 | RFCOMM 入站帧与 localhost IPC 真实收发主机测试 | 没有手机的真实无线电事件 |
| 7 | 手机 MAC、连接代际和有线抢占模型测试 | 原厂蓝牙配对数据归属未验证 |
| 8 | R44 iAP2 SYN/LSP/ACK 真实本机 TCP 协议测试 | 真 iPhone SYN/LSP/ACK 未测试 |
| 9 | 原厂 MFi CONTROL 顺序，**MOCK** 签名/证书测试 | 真实 OEM MFi 服务和 iPhone 认证结果未知 |
| 10 | 蓝牙到 Wi-Fi 交接状态机测试 | 手机与真实热点交接未测试 |
| 11 | 2.4GHz 和能力判定/回退策略模型 | 真车热点配对/网络能力未实测 |
| 12 | Bonjour 身份和监听状态契约 | 原车 mDNS 广播未实测 |
| 13 | 原厂 DIO AirPlay 接收 owner/会话接口约束 | 车机原厂 AirPlay 监听/会话未实测 |
| 14 | 视频音频与媒体流控接口模型测试 | 真视频/音频无线流未测 |
| 15 | SI/DIO 和有线优先状态、断线恢复模型测试 | 实车方向盘、中控按钮和原厂进程回调未测 |
| 16 | 蓝牙→iAP2→Wi-Fi→AirPlay→有线回退全链主机回归 | 整链真实车机没有跑通 |
| 17 | **可生成的原/候选 SHA256+权限固定的 C 备份/回滚清单**；C 事务 trial/recover/负面验收完成 | 真实 SD 介质 fsync/权限、原厂存储分区、车机安装和健康检查/自启恢复挂接未核实 |

**跳过规则**：任何步骤缺少原厂运行证据就标记 `OFFLINE_CONTRACT_ONLY`，继续后面的离线开发；不因为前置缺口停止工作，也不把需要最后实车验收的部分伪称完成。

## R95 实际命令

```bash
bash ci/host_r95_to_step17.sh
python3 -m unittest discover -s tests -p test_r95_sd_plan_codegen.py -v
python3 -m unittest discover -s tests -p test_r95_toolchain_retry.py -v
```

测试完成后主机结果在 `r95_result/R95_STEPS_03_TO_17_EVIDENCE.json`。此文件在源码 ZIP 内，既是证据，也是剩余待验证项。

## 下一阶段（步骤 18～20）

18. 完成真正可撤销的 SD 自动安装入口，但必须在确认本车原厂精确配置路径和原始哈希后生成候选 payload；不能拿测试夹具替代生产清单。
19. 整体候选 ZIP+中文图文教程、静态/编译/离线再验收；原车 iAP2/MFi/蓝牙及 AirPlay 仍须声明未实测。
20. 用户最后一次性上车进行真实 iPhone 配对、音视频、有线优先和断线恢复验收。

`installable=false`; `safe_to_flash=false`; `vehicle_verified=false`; `has_genuine_mfi=false`; `production_callbacks_bound=false`。
