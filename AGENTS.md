# AU57x 固定研发规则 / AGENTS.md

适用于所有后续 AU57x MU1005 无线 CarPlay 研发任务。开始工作之前先读取本文件。

- 优先复用用户已经准备的 QNX 6.5 ARMv7 GCC 8.5 环境、原厂固件、历史构建产物和已经通过的测试，不重复上传、下载固件或重建工具链。
- 已有 Docker 环境优先：本地镜像 qnx65-armv7-toolchain:8.5（旧名 au57x-qnx65-cc:8.5）。如仓库已配置自托管 runner 标签 QNX65_RUNNER_LABEL 或既有镜像 QNX65_EXISTING_IMAGE（不可变 digest），使用该配置。配置是接口约定，不表示当前已经设置。
- 禁止在普通研发工作流中自动 docker build / 重建 QNX SDK。若现成镜像不可达，明确报出缺少的位置或配置；不要悄悄重建，也不要假称已复用。仅在用户明确批准时才另行准备环境。
- GitHub 托管 runner 彼此隔离，不能假定上一次任务中的 Docker 镜像仍在。不要发布或上传未经授权的 QNX SDK / 原厂二进制；缓存和证据遵守许可和仓库访问范围。
- 原厂固件已保存在 GitHub Actions run 45923122041，artifact mu1005-nas-firmware-archive（截至 2026-11-28 已核实有效）；优先复用，不要要求用户重复上传。
- 独立任务并发执行（上限20），不重复运行已通过的测试；蓝牙注册、原厂认证、Wi-Fi 首连、原厂 CarPlay 输出是关键路径。保留有线及回滚，禁止伪造 MFi、未经实车证明的安装声明。
- 汇报用简洁中文：实际完成、剩余步骤、是否能装车；区分已提交、已运行、已通过和实车证明。

## 已核实的 QNX 环境索引（2026-11-28）
- 上游工具链：luka-dev/qnx65-armv7-toolchain，固定提交 baa45224f18ae6b13c2e9c77c6663bb8181eb6a1；Docker target base-env，BASE=base-8.5，linux/amd64，镜像 qnx65-armv7-toolchain:8.5；ARM 编译器 arm-unknown-nto-qnx6.5.0eabi-gcc。
- 历史 GitHub Actions 成功记录：https://github.com/roothongkong-ctrl/AU57x-R22-QNX65-Build/actions/runs/46098063669/job/140833939988 。日志确认 GitHub 托管 ubuntu-24.04，构建步骤 #8 至 #28 全部 CACHED；已有缓存 scope=mu1005-qnx65-baa45224-gcc85-v1（type=gha），成功来源分支 mu1005-si-cprx-real-review。缓存有分支可见性和有效期限制，scope 相同并不保证跨分支命中。
- 复用优先级：现成且经验证的本地镜像/已配置的固定镜像 > 在原缓存可见分支恢复已有缓存。允许恢复已构建层，不等同重新编译 SDK；缓存不可达时停止并报告，不自动冷构建。尚未找到经验证的永久完整 Docker 镜像地址；不能把缓存当作永久环境文件。
- 已确认失败记录：https://github.com/roothongkong-ctrl/AU57x-R22-QNX65-Build/actions/runs/46099734042 ：新分支临时 runner 没有可访问的现成镜像，禁止重建规则正确阻止了执行。
- 新会话接手：先读取 main/AGENTS.md，再查上述成功日志、最新分支及 Actions 实际运行状态。项目基线 MHI2_CN_AU57x_K1006_2 / MU1005，当前关键路径 2/10 已完成，第3步原厂蓝牙接入未证实；Congo 蓝牙直接依赖 libc/libsocket，不依赖缺失的4个 NVIDIA 图形库；继续独立蓝牙验证。不要把模拟运行当作 iPhone 实际首连。进度以最新可验证证据为准。
