# R105.063 — 阶段04–07提前整合 (NO INSTALL)

此前 native-library 仍从 R85 7f595231 构建 cprx_service.c/h，而 current HEAD 的同名文件已包含首视频/可选通道更新，主机 first-video 用例使用当前源码。两者 blob 不同，其余八个 AirPlay 源文件相同。改为 native-library 使用当前检出的 AirPlay 源码；保留 R85 参考版本及差异记录、源码 SHA256 清单、原生 ELF 校验、NO INSTALL 标记。不会覆盖真实车机文件。

目的是避免后续统一包把旧接收器与新 SI 组合。尚未完成原厂进程接入、真实手机认证、真实无线画面或安装验收。当前 ARM 构建需云端验证，最终安装包仍禁止发布。
