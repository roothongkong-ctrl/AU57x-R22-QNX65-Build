# R105.163 编译环境一次构建、多次复用

R105.159实际日志：GNU源码缓存命中，但GCC build stage #18 约301秒仍在编译。新 composite action .github/actions/mu1005-qnx-image/action.yml 使用锁定上游baa45224、现有GNU原始校验及脚本哈希的精确缓存键，完整docker save/load，校验压缩包哈希、镜像ID、架构、GCC版本、实际ARM ELF编译及mkifs。第一次缺缓存才构建，并在下游测试前保存。新runner必须真实命中才能通过独立验证；缓存丢失或损坏不得假装成功。只缓存已使用的公开上游工具链构建环境，不含车机固件/凭证/用户文件，不发布镜像或SDK下载。缓存有容量、访问和过期限制，不能承诺永久保存或固定提速。后续QNX任务复用该action，保留原始guest测试和独立boot材料，矩阵在warm job成功后启动，避免每路独立冷构建。现有R105.161不取消；未修改正在运行的workflow。新机制须验证后再替换后续任务。
