# 下载

## 稳定版本

生产环境推荐直接使用 **[GitHub Releases](https://github.com/alibaba/loongcollector/releases)** 上当前最新 **stable** 发行版附带的预编译包（tag、`loongcollector-<version>.<os>-<arch>.tar.gz` 及 `.sha256` 均以发布页为准）。

国内用户也可以从阿里云 OSS 下载，满足如下模式（具体是否沿用、路径是否与 Release 正文一致，**以每条 Release 的说明为准**）：

`https://loongcollector-community-edition.oss-cn-shanghai.aliyuncs.com/<release_version>/loongcollector-<release_version>.<os>-<arch>.tar.gz`

**不要依赖本文档中写死的旧版本号**；更新文档或脚本前请打开 [latest release](https://github.com/alibaba/loongcollector/releases/latest) 核对文件名与校验和。

## 容器镜像与 `latest`

- **GHCR**：[`ghcr.io/alibaba/loongcollector`](https://github.com/alibaba/loongcollector/pkgs/container/loongcollector) 在每条 Release 中通常提供 **`<版本号>`** 与 **`latest`**（`latest` 对应该发布渠道的当前最新镜像，以 Release 正文为准）。
- **阿里云镜像**（`sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector`）：与 Release 正文同步的是 **带版本号的 tag**；**不要假设存在可用的 `:latest`**——编排与 `docker pull` 请写 **明确版本号**，或改用 **GHCR 的 `:latest` / 具体 tag**。

## 开发版本

对于有兴趣测试最新版本或者乐于贡献代码到开发者，获取最新开发代码GIT仓库的命令为：

```bash
git clone https://github.com/alibaba/loongcollector
```

注意，main分支是 LoongCollector 的开发分支。因此，存在无法编译或者运行时出错的可能。

我们欢迎更多开发者参与测试或者开发，您的贡献将使 LoongCollector 更加出色。
