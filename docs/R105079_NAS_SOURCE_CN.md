# R105.079 自有 NAS 固件源
用户提供 NAS 分享链接。链接含访问令牌，不提交到公开代码或 workflow 输入。仓库 Actions Secret 名 MU1005_NAS_URL，由仓库所有者填写完整链接。当前连接器无 secret 写入或 workflow dispatch 接口。缺少 Secret 时任务明确失败，保存后可重跑失败任务。HTTPS 证书验证保持开启，日志只输出 HTTP 状态和字节数。7z 签名、完整性、SHA256、候选镜像提取沿用现有工具。尚未获得固件，不声明下载成功。
