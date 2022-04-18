# iLogtail 开发者指南
通过以下内容您将了解到iLogtail 的整体结构以及了解如果学习贡献插件。

## 插件贡献指南
  - [如何使用Logger](How-to-use-logger.md)
  - [开发Input 插件](How-to-write-input-plugins.md)
  - [开发Processor 插件](How-to-write-processor-plugins.md)
  - [开发Aggregator 插件](How-to-write-aggregator-plugins.md)
  - [开发Flushor 插件](How-to-write-flusher-plugins.md)
  
## 测试
当完成插件编写后，以下内容将指导你如何进行单元测试以及E2E测试，E2E 测试可以帮助你进行mock 测试环境，比如Mysql 依赖等。
- [如何编写单测](How-to-write-unit-test.md)
- [如何编写E2E测试](../../../test/README.md)
- [如何手工测试](How-to-do-manual-test.md)

## 增加插件文档
- [生成插件文档](./How-to-genernate-plugin-doc.md)

## 代码检查
在你提交Pull Request前，你需要保证代码符合规范，并保证测试通过，以下内容将帮助你进行相应检查。
- [检查代码规范](How-to-chek-codestyle.md)
- [检查代码License](How-to-check-license.md)
- [检查代码依赖License](How-to-chek-dependency-license.md), 如无新增依赖包，请忽略此步。
- 使用`make test` 执行所有单测，保证单测通过。
- 使用`make e2e` 执行e2e测试，保证E2E测试通过。

## 其他
- [如何使用docker编译镜像或CGO 动态链接库?](How-to-build-with-docker.md)
- [如何发布 iLogtail?](How-to-release.md)
