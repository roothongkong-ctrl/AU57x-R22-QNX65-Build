# R105.136 首项并行定向实验（NO INSTALL）
R105.134 run 45977348223：USB NOW 成功，driver NOW/LAZY 均失败，错误类别 symbol_resolution；旧解析器将复数 symbols 尾字母误当具体符号 s，不能据此认定缺失名称。修复边界并增加回归；规范化 QNX CR 字符后再做精确 marker 验收。新建独立静态动态导入/导出候选检查（四线程，实际原厂 mm-ipod、两个插件、已准备库及 SDK 基线），只比对精确符号名称，不提供伪造符号，不改变认证。若 driver 依赖宿主导出，独立 witness 的失败不能替代真正 mm-ipod 运行结论。
R105.130 btstack 与 bluetooth 各有 missing1；新增两个并行 SDK 定向 lane，仅在缺失项确实为同名 libecpp-ne.so.4 且 pinned SDK ABI/SONAME/依赖检查通过才继续真实 QNX 运行；不覆盖原厂库，不发布 OEM 文件，不伪造蓝牙硬件。R105.135 SI 已运行，保持不变。现有正常集成测试不另行重跑。首次连接仍未验证。
