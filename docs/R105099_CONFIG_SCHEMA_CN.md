# R105.099 精确原厂配置结构
R105.097 两条 discovery 均 success；真实 EFS presets/bluetooth.json (1434 bytes) 与 production/connectivity.json (8699 bytes) 已定位，先前解析拒绝未知结构。新增只读精确 SHA 校验、JSON/带注释及尾逗号解析、递归 enableIap 类型与路径报告；不输出其他配置值，不猜生产覆盖顺序，不修改原厂配置。只解 stage2/EFS，不重复 app70、不访问 NAS。先跑四个合成解析断言，真实结果待云端。R105.098 符号任务及 R105.096 两条 QNX 仍运行。NO INSTALL。
