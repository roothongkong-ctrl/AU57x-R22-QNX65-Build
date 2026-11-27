# R105.172 五项统一复用（NO INSTALL）
按用户要求将5项依赖原厂功能的开发方式统一，不重新实现原厂服务，不增加无关回归。
- 10：采用现有 tools/mu1005_efs_launcher_stage.py 包装器（原样exec参数、唯一配置字段变更，R105.159/161/164五种QNX转发通过）。限定离线启动适配完成；实际原厂加载第17项，路径第9项，生效第11项。
- 29：采用 project/source/src/cp_step04_stock_mfi.c 的原厂已加载libairplay证书/签名接口；保留原厂认证及有线排他。已有API适配，不新增第二个认证实现；缺真实owner、现存句柄与释放契约，后续同会话确认，不能宣称认证已接通。
- 35：采用 integration/mu1005_si_native/ap_live/ap_live.c 的原厂uaputl只读查询，已有2.4G基线、双快照和ARM编译；写入及独占权依赖真实AP owner，当前不执行写入。缺真实owner证据。
- 46：采用原厂DNS-SD库及原厂daemon，历史create连接失败；本轮只定位原厂daemon和启动描述、候选socket，不重复原来已失败的缺daemon环境调用。
- 51：采用 project/source/legacy_mu1005/airplay/native_exports.c 和现有SI/CPRX接收器适配；真实owner、server、已加载库、授权和无线接口仍需接入，禁止新接收器或替代原厂授权。
- 统一新只读观察工具 lab/mu1005_stock_reuse/observe.sh，只输出固定路径候选和查询可用性，不执行硬件查询、认证、启动、写文件，不打印密钥或配置。FD出现不是owner证明，缺失不是不存在。与第9项既有probe及后续首连联合使用，非部署包。
- R105.172仅2路：新观察工具夹具测试、缺失的DNS-SD daemon位置定位。其他现有已通过测试不重复。全部真正接入及实车验证保留原编号；63/64前置，55-59联合验收。无真实认证或无线连接结论。
