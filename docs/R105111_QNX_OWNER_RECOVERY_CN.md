# R105.111 原生QNX首连通过后，扩大至进程边界和有线恢复
R105.108 run 45947911414：ack-hold job 140336756724 成功，完整first-connect/video/restore fixture且进程退出0；ack-control job 140336756804 发送8字节成功后client_owner_denied errno257，原版失败。R105.110 Linux两路均成功，R105.109真实原厂配置整合通过。原厂回调、手机和视频仍是fixture，不得宣称实车认证/画面。
本轮选择已有真实fork进程边界与独立SI/DIO有线接管fixture，在QNX ARM分别运行；修正fixture的终止条件，等待实际服务端路径释放，而不是仅backend恢复即退出，保留独立owner、session、状态、原厂恢复等所有断言。现有生产ACK修复不改。旧R105.096 RAM盘仅保留手动触发，避免新提交重复失败路线。NO INSTALL。
