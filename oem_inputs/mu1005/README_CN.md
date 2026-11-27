# MU1005 原厂输入
固件 MHI2_CN_AU57x_K1006_2 / MU1005。此目录仅保存研发输入，不是安装包。
用户选择在现有公开仓库中管理原厂输入。未独立确认固件再分发许可。
将已校验的 MU1005_private_OEM_seed.zip 上传到本目录；文件名不变。
运行 python3 oem_inputs/mu1005/verify_seed.py /tmp/mu1005-oem-extracted 可验证并提取已固定哈希的 btstack。
seed 仅包含 btstack，不含 11 个依赖二进制。不得把校验成功当作原厂蓝牙运行成功。
现阶段自动云端验证只检查文件大小和 SHA256。原厂运行环境需另行适配。
