# R29 完整传输插件云端源码输入（NO INSTALL）
本目录五段为原有 R29 源码及测试的 xz tar 源码包（不含原厂二进制，不含可执行文件）。通过逐段 Git blob hash 与全包 SHA256 核对，再用 `sh unpack.sh /tmp/r29-full` 展开源码。先运行 test.sh，再使用锁定 QNX ARM 编译器执行 build_qnx_review.sh。主工作流保存完整展开源码、共享库和 ELF 证据。原有六路并行测试继续运行。
本轮修正：已有部分帧写入物理 socket 时，世代替换或断开将整个流标记不可复用；尚未写出的旧队列仍能清空。新增两种断线回归，避免旧帧截断与新帧拼接。本地 ASan/UBSan 和共享库严格链接均通过。
OEM 插件入口仍拒绝自动启动（未取得真实 owner/清理契约），不能将链接成功视作实车接入、真实认证或重连验证。不能安装。完整源码包 SHA256 0e15785816b5adfdb643fb12f89fb97ef7db3883cd7e0abd4bedce1da961c821。
