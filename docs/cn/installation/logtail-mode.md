# LoongCollector 的 Logtail 兼容模式使用指南

LoongCollector 提供了 Logtail 兼容模式，可以让您在升级到 LoongCollector 后继续使用原有的 Logtail 配置和数据，实现平滑迁移。本文将详细介绍如何配置和使用这个兼容模式。

> 在开始之前，请先了解 [LoongCollector 的目录结构说明](loongcollector-dir.md)。

## 为什么需要兼容模式？

由于 LoongCollector 采用了新的目录结构和配置体系，与原有 Logtail 存在差异，如果您相关的目录文件升级迁移困难，可以选择使用 Logtail 兼容模式。启用兼容模式后，LoongCollector 将：

- 保持与 Logtail 相同的目录结构

- 继续使用 Logtail 的自定义目录配置方式

- 继续使用 Logtail 的文件命名格式

## 配置方法

### 1. 主机环境配置

您可以通过以下两种方式之一启用兼容模式：

**方式一：命令行参数**

```bash
./loongcollector --logtail_mode=true
```

**方式二：环境变量**

```bash
export logtail_mode=true
./loongcollector
```

### 2. 容器环境配置

此前的 Logtail 容器镜像中，Logtail 运行时目录为 `/usr/local/ilogtail`，而 LoongCollector 运行时目录为 `/usr/local/loongcollector`。

因此，在容器环境中，除了启用兼容模式外，还需要调整目录映射。请按照以下步骤操作：

1. 需要给LoongCollector容器添加环境变量：

```bash
logtail_mode=true
```

2. 需要调整LoongCollector挂载路径映射：

将所有 `/usr/local/ilogtail` 路径替换为 `/usr/local/loongcollector`：

```plaintext
# 常用目录映射示例
数据检查点：
/usr/local/ilogtail/checkpoint → /usr/local/loongcollector/checkpoint

采集配置目录：
/usr/local/ilogtail/config → /usr/local/loongcollector/config
```

3. 修改容器镜像地址为LoongCollector镜像地址

`sls-opensource-registry-vpc.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector:版本号`

## 迁移建议

为确保平稳迁移，我们建议您：

1. 先在测试环境进行充分验证

2. 选择业务低峰期进行升级

3. 做好配置和数据的备份

4. 逐步迁移，避免一次性升级所有实例

5. 密切监控日志采集状态

> **注意**: 迁移过程中请确保数据完整性,建议先在测试环境中进行测试，并非高峰期进行升级操作。