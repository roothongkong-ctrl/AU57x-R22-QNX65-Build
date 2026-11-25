# MU1005 阶段1：修正原厂 SDP 半成功错误路径（NO INSTALL）
上一版 c167ac5d 的 GitHub Actions 45574119238、45574119121 均 success。第一阶段 donor ARM 编译/链接已完成；不等于原厂 SDK 已运行。
本轮在用户原始 btstack（本地原始文件 /mnt/data/btstack(1)）直接执行 llvm-objdump 复查：
- 0x207244 SDK SDP_ADD，0x207290 检查是否已入链，0x2072b4 实际插入，0x207334 后 0x207338/0x20733c 仍可能非零返回。插入之后返回错误不等于没有注册。
- 0x2071c4 SDK SDP_REMOVE，0x2071f8 检查在链，0x207218 调 0x1cc920，先脱链，0x207228 后 0x20722c/0x207230 仍可能非零返回；0x1cc934/0x1cc93c 将 record 的 next/prev 清零。
- 修改 donor port：两份 SDP record 在调用 ADD 前标记资源尝试；失败路径也回收已尝试的记录。REMOVE 后检查原厂 intrusive links，保留非零状态记录，记录返回错误；若 SDP 仍有链接不撤销其 RFCOMM channel；尝试再请求原厂 EIR refresh。静态储存不重用。链字段读取仍需原厂生命周期串行化才能成为可靠释放证据；EIR 请求不是 controller ACK。该修正不解决真实原厂线程/锁/生命周期，也不代表发现成功。
- 本轮不扩展 Wi-Fi、认证、AirPlay、安装，不启动 OEM，不上传 OEM 文件；当前云端只有通用 QNX 与原厂接口模拟，不具备真实蓝牙硬件。下一关键证据必须是原厂 SDK 启动、无线注册/广播及关闭实际运行，而非新增协议或普通模拟。未有这些证据之前，仍 NO INSTALL。
