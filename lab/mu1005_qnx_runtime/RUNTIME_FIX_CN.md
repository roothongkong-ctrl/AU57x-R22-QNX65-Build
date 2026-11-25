# 具体故障修复
45549072849 串口确认 io-pkt-v4-hc 在 cache_init 返回 ESRCH，rloop init:3，pidin 有进程但无 socket。依据 QNX 文档的 CACHE_NOP 及 LuKa QEMU RAM 环境设置，仅在云端无物理 DMA 的实验网络进程设置 CACHE_NOP=1；其能否解决 cache_init 尚待验证，非车机设置。
完整链在 bootstrap_chain_fixture.c:90 cp_stock_mfi_bind 断言失败。原先 dlopen(NULL) 在 QNX 无法确定能解析主程序测试符号。现把已有纯测试提供者拆成显式独立 DSO（无真实证书、密钥、签名），保持原厂接口绑定代码不变，通过真实动态加载器解析，记录返回码及实际调用次数。仅实验 IFS，禁止安装。