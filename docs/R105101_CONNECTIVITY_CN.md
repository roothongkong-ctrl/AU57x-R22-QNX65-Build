# R105.101 配置语法与启动引用两路并行
R105.099通过但不是配置可用：preset无enableIap，production/connectivity.json有一处，标准JSON及普通注释解析失败。新增只读精确哈希词法诊断（仅输出语法类别及目标布尔/数值候选）和独立stage2/EFS启动引用定位（路径、哈希、关键字，不输出配置原文）。不猜预处理条件或生效顺序，不修改文件。R105.096两个内存盘方案均失败；demand串口确认xpt_configure缓存初始化失败，暂停重复该路径，不降低AF_UNIX权限校验。R105.100五线运行中。NO INSTALL。
