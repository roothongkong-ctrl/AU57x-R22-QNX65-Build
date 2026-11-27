# R10：高德白色卡片缩小70%，最小修改可行性核验（不安装）

## 固定目标
- 原车 2017 A6L C7 / MHI2_CN_AU57x_K1006_2，MIB Cluster Map v1.2.14。
- 高德原版白色卡片及箭头、距离、路名缩至原尺寸 70%；地图、中心屏幕、原车功能不变。
- 用户没有额外手机解包数据；只基于现有 MIB 源码。

## 逐文件确认

| 已有功能 | 来源 | 实际作用 | 能否只缩白卡 |
|---|---|---|---|
| `CLUSTERMAP_CARPLAY_SCALE` | config/gal_dualscreen.conf | 更改 iPhone 认为的物理屏幕尺寸，整个仪表 UI 文字、图标和卡片一起变小 | **否** |
| `CLUSTERMAP_CARPLAY_FRAME_SCALE` | 同上 | 把整帧视频缩小 | **否** |
| `CLUSTERMAP_CARPLAY_VIEW=map/full/card` | 同上 + thirdparty/.../libaltscreen111_gen2.c | 从三种内容中选择一种作为当前 type-111 视频 | **否** |
| `CLUSTERMAP_CARPLAY_CARD_POSITION` | 同上 | `maneuverLayout` 调左右/上方位置，可能影响地图锚点 | **否** |
| `CLUSTERMAP_CARPLAY_INSETS` / `safeArea` | 同上 | 改 iPhone 安排仪表重要 UI 的安全区域，**不是只改变卡片宽高** | **未验证，不可当作70%** |
| `player/opengl_gpu.cc` | 原版播放器 | 解码已经合成的 H.264 画面 | 只能裁切缩放可见卡片，无法恢复被它挡住的原始地图 |

官方高德 iOS-HelloCarPlay 示例里的 `topInfoBgView` 是第三方 app 源代码中可独立布局的视图，不是车机收到的分层协议。不能把示例代码的尺寸直接用到正式版高德。

## 独立出处

- [MIB v1.2.14 源码](https://github.com/MacsInSpace/mib-cluster-map/tree/v1.2.14)
- [CarPlay 大小与显示模式说明](https://github.com/MacsInSpace/mib-cluster-map/blob/v1.2.14/docs/CarPlay.md)
- [官方高德 CarPlay 开发示例](https://github.com/amap-demo/iOS-HelloCarPlay)
- [苹果 WWDC19：ViewAreas 调整个仪表显示区域，多流才能把地图/卡片分开呈现](https://developer.apple.com/videos/play/wwdc2019/252/)
- [苹果 CarPlay HIG：CarPlay 自己按屏幕信息缩放 UI](https://developer.apple.com/design/human-interface-guidelines/carplay)
- [2026年CarPlay第三方研究：ViewArea/SafeArea并不是视频裁剪；maneuverLayout会移动地图锚点](https://github.com/harman-f/mhi2_altscreen_carplay/blob/main/docs/architecture/NAVIGATION_COMPOSITION_AND_SAFEAREA.md)

## 结论

**不能把“配置项缩70%”当成一个能保证达到用户目标的安全一文件修复。**
v1.2.14 当前公开接口里没有“仅白卡片宽高×0.70”的控制。Apple 未公开的私有内部实现未被此源码核验覆盖。

R9 的软件局部缩放测试虽已通过12/12，但必须有另一份实时、无卡片、与当前视频同一时刻的完整地图做背景；当前 v1.2.14 没有这样的同步背景视频。因此 R9 **不能安装**，也不能借安全区域参数冒充已完成。

## 下一步仅保留两条真实路径

1. **优先且改动小**：寻找新证据/公开实现确认苹果对 `maneuverLayout`/Card 控件存在**独立大小协议**，有确凿字段及高德支持证据才写 patch（目前没有）。
2. **如果没有**：要么接受地图背景不精确的合成妥协（不推荐车载导航），要么设计两路/新鲜完整地图背景及原卡合成（已知复杂，不能称为一文件补丁）。

**无实车可用70%成品；禁止生成“已完成”SD安装包。**
