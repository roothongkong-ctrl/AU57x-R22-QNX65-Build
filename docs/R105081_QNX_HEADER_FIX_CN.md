# R105.081 QNX 头文件修复
R105.078 host startup watcher 测试通过，但 QNX ARM 编译报告 size_t 未定义。按已通过 ARM 编译的 bt_startup_snapshot.h 的包含顺序，在 bt_startup_watch.h 先引入 sys/types.h，再 stdint.h、stddef.h、latch header；新增独立 QNX 编译器头文件语法检查。修复已提交，须等待新 ARM 编译结果；未修改真实 OEM 启动或安装逻辑。NAS 任务独立执行，不重复触发下载。
