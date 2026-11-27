# R12：保留完整地图，原版白色卡片70%（实际合成代码，仍非安装版）

你要的是：地图不放大、不缩小、不改位置；原版高德白色箭头、路名、距离卡片缩到70%。

## 已实现（可云端运行测试）

`dual_stream_joiner.h` + `test_joiner.cpp`：

- 收**111纯地图帧**和**112原版卡片帧**两份独立YUV420画面。
- 检查来自同一个CarPlay会话、尺寸为800×298、两帧时间差最多100毫秒、距离现在不超过200毫秒。
- 调用R9原有的卡片70%像素缩放，将卡片原版像素贴到完整地图背景上；卡片以外，YUV每一个像素与纯地图相同。
- 任一路消失、连接重建、旧帧、乱序、尺寸不符时拒绝合成，避免旧导航卡片误导。
- 不触碰110中控屏，保留原生路径。

## 目前明确无法解决

**真实iPhone上的高德是否输出type112的独立卡片帧，仍无实测。** 苹果WWDC19只明确说明两路并行地图/转向卡片视频可用于Apple Maps；不能从这个推断高德已实现两路。当前R8的112收发会话有宿主机模拟，但尚未连接真实MIB v1.2.14的AirPlay /info/SETUP、会话AES、QNX解码器。

这个项目只验证**两路原视频存在时如何正确合成**，并非已经取得真实地图+高德卡片。

## 待完成的实际集成

1. 在v1.2.14的AirPlay advert/SETUP中安全接入R8的112（实验默认关闭，保留110/111原样）。
2. 真实112视频AES解密+H264解码成独立YUV帧。
3. 111强制map、112选择instructioncard，调查iPhone高德是否发起两路。
4. 接入本R12合成器，构建QNX ARM二进制、安装/回滚并做实车确认。

没有证据前，**禁止交付伪装成可上车用的安装包**。

来源：
- 苹果 [WWDC19](https://developer.apple.com/videos/play/wwdc2019/252/)：可同时渲染地图/卡片到两路视频；这是协议能力，不是高德兼容承诺。
- [MIB Cluster Map v1.2.14](https://github.com/MacsInSpace/mib-cluster-map/tree/v1.2.14)
- [lvalen91/ocbm](https://github.com/lvalen91/ocbm/blob/main/docs/carplay/06_AV_PIPELINE.md)：112尚未在其真实CCPA抓包中出现过。
