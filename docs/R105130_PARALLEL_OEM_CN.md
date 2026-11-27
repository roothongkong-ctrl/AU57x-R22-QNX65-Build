# R105.130 十二路真实 OEM 运行并行
R105.127 多个任务的实际编译错误为 QNX process.h spawnv 要求 char * const argv[]，不是 const char *const*；直接传入原 char **，不关闭 Werror。R105.129 45972295495 已确认 7 项布局、ISO 3430 个条目/3429 资源、无候选容器、原始树一个符合 SONAME/符号的 ICU ELF；新增 SONAME fallback 并保留歧义拒绝。SI/DIO 原始配置增加 layout 对照，uaputl 仅执行 BSS 状态查询，不启动 AP 或伪造关联。
一个 compiler-preflight 先验证公共启动器并填充已有 buildx cache；然后 matrix 12 独立任务 max-parallel=12、fail-fast=false，实际并发受 GitHub 账户、额度和 runner 调度限制。每路只测试一个模式，不重复写编译缓存；缓存命中不保证，缺失仍需编译。测试原始 ELF 进程是否执行/退出/存活；不是服务就绪、无线 MFi 或真实 iPhone 成功。私有 runner 原厂文件不发布，NO INSTALL。
