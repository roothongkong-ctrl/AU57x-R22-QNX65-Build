# R13 / 真实 CarPlay 接收层研究（源码实验，不可安装）

## 本轮关键突破
R8 的112端口是宿主机模拟，不连真实AirPlay。本轮的补丁将实验接入MIB Cluster Map v1.2.14真实CarPlay hook。

- patch_real_v1214.py: 仅修改官方源 thirdparty/mhi2_altscreen_carplay/libaltscreen111_gen2.c
- r13_112_probe.inc: 向/i​​nfo加入112独立UUID与instructioncard URL；SETUP隔离原厂请求，提供独立TCP监听；使用原库屏幕AES-CTR解密、统计收到的配置和帧；TEARDOWN清理私有会话。
- 与原版111视频和110中控隔离，不绘图不修改实际地图；限制最多120帧，仅记录数量而不收集地图内容。

## 双重安全开关
只有同时满足编译宏 CLUSTERMAP_R13_112_LAB 和环境变量 CLUSTERMAP_R13_112_PROBE=1 才可能向手机公布112。
默认构建没有实验功能；本次GitHub只编译，不对车辆安装，更不实车启用。

## 不能误称成功
1. 苹果WWDC19明确讲的是Apple Maps可以提供并行map/card两个流，不保证高德正式App也支持。
2. 没有真实高德/iPhone 112通信记录，代码能编译不等于手机真的发送了112。
3. 还缺112视频解码成I420、卡片实际边界校准、真实音视频同步、QNX ARM交叉编译、实车验收。
4. R12云端6/6过的是双路模拟帧合成，不是高德车载功能验收。

参考：https://developer.apple.com/videos/play/wwdc2019/252/
参考：https://github.com/MacsInSpace/mib-cluster-map/tree/v1.2.14
R13验收：编译真实v1.2.14 AirPlay hook的默认与实验分支；单文件补丁检查；不出安装包。
