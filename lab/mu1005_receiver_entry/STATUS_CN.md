# R105.311 复用 donor 自带启动器
R105.309 实际 QNX 两 receiver 都在 ip=0x0103f988 mapaddr=0x3f988 ref=0x00f7fff4 SIGSEGV；R105.310 两个均 166/167 SDK 强符号匹配，未解析 v75gen。原项目 wcp_ctl.sh 明确必须通过 /mnt/app/carplaymaps/wireless-receiver-exec 进入，而前一轮直接启动裸 receiver，因此不应认定原项目正常启动路径失败。新增只读检查：遍历固定公开 donor 的 payload ARM ELF，找 v75gen 真实动态导出/重定位、启动器公开路径及故障地址可能映射。先找真实 provider，不实现假符号、不改校验或认证，不重跑 QEMU/SDK；若只能使用未公开组件则明确阻塞或转回原厂 DIO/Congo。当前故障与 v75gen 因果尚未证明，NO INSTALL。

## R105.312
R105.311 run 46141911997 因脚本命名 inspect.py 遮蔽 Python 标准库 inspect，导致 pyelftools 循环导入，尚未进行 ELF 分析。原子重命名脚本、移除原文件并更新工作流，保留分析逻辑；不重跑 QNX、不修改 donor 或原厂程序。