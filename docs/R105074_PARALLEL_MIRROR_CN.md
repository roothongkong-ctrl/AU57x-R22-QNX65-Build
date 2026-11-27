# R105.074 并行与镜像源
MIB Solution 实际 HTTP 530、0 字节。替代目录 https://mibvault.com/files/MHI2%20-%20HARMAN/Firmware/Audi/CN 列出同名 914 MB 原厂档案。通过公开 Download 控件单次下载，遇到限额/登录/交互阻塞停止，不绕过。完整档案 CRC 检查并记录 SHA256；同名不是内部身份验证。尚未运行成功。
将 R105 iAP2 host 原串行任务拆为 upstream-link、oem-contracts、airplay-envelope 三个独立任务，汇总依赖这三项。保留现有统一九任务、QNX ARM 单任务和 SI 接收任务，使用标准 runner；不人为扩充重复任务。QNX OEM 硬件及实际手机认证仍未验证。
