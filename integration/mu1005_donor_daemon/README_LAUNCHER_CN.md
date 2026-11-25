# 真正原厂启动配置已找到（NO INSTALL）
先确认 1edbd61a 的 45568482338、45568482359 均 success。此次从用户历史真实文件恢复启动配置，解除之前 R94/R100 持续缺少原厂配置的阻塞。
- mifs-stage2(3).img 44,040,192 字节：LZOZ，2MiB block，header 0x800，每块 512 对齐，30块 lzo1z 解压成功，原始 imagefs 61,587,580 字节，SHA256 43aa81946b8bd513eab0d804a99a85e6ff99f3b36b9a39b2ee68ae1625b9140f。未发现 connectivity.json 字面内容。
- efs-system(4).img 2,097,152 字节，SHA256 96fbdddae01ec4e785104e9516f0a84706ff6c2b8c912c2c33ad066988bd8244：文件名 0x84e48，正文 0x84e80..0x8707b（end exclusive），8699字节，SHA256 56c14940455215233f9ead5a8cd3e3a9dc984f372b0d7421ba26ff8e8f579470。尾部填充 FF，后接另一个文件记录；原厂文本以 # 注释，不能直接 json.loads。
- 真正启动节点 launcher.btstack，而不是根 btstack（后者是 SDK 参数）。原厂 exec /eso/bin/apps/btstack，customer exec /eso/bin/PhoneCustomer/btstack，preCondition /tmp/mvloaded，maxUnhealthyReports 3，onFailure 调原厂 /eso/bin/reset_bt.sh；initRunMode=201216，btstack bitValue=131072（非实例ID）。根 bluetooth.enableIap=false。以上来自实际原始字节与解析，不是上游猜测。
- 新工具 tools/mu1005_efs_launcher_stage.py：只接受精确镜像 SHA 和配置 SHA，保留原始文件及 # 注释，只改 launcher.btstack.exec 一处；解析前后语义相等检查，其余启动/恢复/customer 配置不变。不写 EFS，不切换 enableIap。生成 review-only wrapper 默认 exec 原程序并保留参数，明确 opt-in 且 probe 库可读才设置预加载。它只是 probe 入口，不是无线服务自动启动。
- 本地真实原始 EFS 验收：提取、逐字节反向恢复、原始镜像 SHA 保持不变、三种不匹配拒绝、shell 语法检查均通过。staged config SHA256 2cf6a5339deeafa1f0636475f23eb0ace83c891f0b41e2e5b127939d876bcc82。未公开用户原厂镜像或配置全文。
- 尚未确认当前车机实际运行配置目录，未证明 factory launcher 环境继承及 btstack 实际 preload/无线服务绑定；不得装车。下一步用这份真实配置核实原厂实际启动入口并接真实启动/退出，不能把 offline staging 当作完成第一步。
