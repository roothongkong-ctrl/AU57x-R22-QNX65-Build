# 集中推进：独立网络 worker 与云端编译缓存（NO INSTALL）
- 复用上游 LuKa 20c94f51 的独立 socket worker 思路，不移植其未经 MU1005 验证的 SDK 跨线程调用。已有 connected 函数提供真实 loopback、非阻塞 fd，新增独立 pthread producer，与 original-first notify 的 owner-only mailbox 串联。只在无线服务 EMPTY 且 wired idle 时取/请求 fd；保留真正 daemon identity witness，失败 preflight 关闭未转移的 worker fd，歧义 attach 不释放。worker 不执行任何 SDK 调用。外部 supervisor 启停、同步生命周期与实际定期唤醒尚未实现；没有自动加载、没有实车绑定。
- 云端 ARM 统一工作流之前每次从头 docker build GCC；与已有 QNX runtime 使用同一固定 toolchain revision/GNU source 校验和缓存 scope，通过 buildx/build-push-action 的 GHA cache 复用编译层，仍重编当前工程所有 ARM 源码。扩大 push 路径覆盖源码/测试/CI，避免修改未触发构建。第一次是否命中及耗时改进待云端结果；不宣称已测得速度。
- 新增真正 TCP 两次 socket producer 测试、ARM 导出检查。原厂 0x27a1b8 的直接 B/BL 调用在 0x1e4728、0x1e47a0、0x1e4a98、0x1e4b74、0x1e4d8c、0x27a7d0；虽然可作为后续原厂唤醒研究候选，尚未验证它对跨线程周期调用的安全性，不从 worker 擅自调用。
