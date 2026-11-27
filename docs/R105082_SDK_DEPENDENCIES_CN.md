# R105.082 原厂依赖库真实输入与 QNX SDK 对比
R105.081 ARM 编译修复已通过，统一九项+SI 四项全部通过；R105.077 QNX joint 运行也已通过。新增以真实已上传 btstack 为输入，查找 SDK 的同名依赖、核对 ELF32 ARM/SONAME、动态导出与 OEM 强引用覆盖率，并报告原始 trace 符号候选来源。SDK 同名库只作候选，不能替代未证实 OEM ABI；不执行 btstack、不打包 SDK 库。NAS 任务仍卡在 checkout，未取得下载结果。本次新检查待云端运行。
