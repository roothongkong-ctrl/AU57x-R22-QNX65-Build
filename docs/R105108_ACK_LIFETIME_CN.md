# R105.108 QNX ACK 生命周期修复
R105.107 host、QNX双向真实AF_UNIX身份/数据回环、QNX本地DIO fixture启动/恢复三路通过；完整首连失败，starts=1、client phase=5，故障收敛至应答。假设QNX在server close后getpeername失效，未宣称已证实。实际生产候选代码只对QNX v3连接增加应答发送完成后的有界保留，等待client关闭；保留原有期限、身份/owner检查、拒绝额外请求和恢复语义，旧行为作为编译控制组。两路完整QNX首连并行，另开host回归。诊断只注入临时runner副本。无认证绕过，未实车验证，不可安装。
