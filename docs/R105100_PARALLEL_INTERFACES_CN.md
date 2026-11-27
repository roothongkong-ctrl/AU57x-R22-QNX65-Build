# R105.100 原厂五路独立接口分析
R105.098 已通过，原厂 btstack 强导入匹配213/364，trace provider 为 stage2/eso/lib/libiplcommon.so。R105.099 配置和 R105.096 两路 QNX 继续运行。新增五路独立 runner：SI、DIO、libairplay、原厂 bluetooth、原厂 iap2_driver；使用精确原厂 SHA 验证，分别列出实际接口候选、直接依赖及既有原厂 inventory 可提供的强导入。未覆盖的依赖明确标记为不在旧 inventory，不表示固件不存在；不猜运行时绑定，不安装。五路共享已校验的 GitHub archive 来源，runner 分开提取，未重复请求 NAS。测试未返回前不计新增正式验收。
