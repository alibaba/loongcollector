# 贡献指南

欢迎来到 iLogtail 的社区！感谢您为iLogtail贡献代码、文档及案例！

iLogtail 自从开源以来，受到了很多社区同学的关注。社区的每一个 Issue、每一个 pull request (PR)，都是在为 iLogtail 的发展添砖加瓦。衷心地希望越来越多的社区同学能参与到 iLogtail 项目中来，跟我们一起把 iLogtail 做好。

## 行为准则

参与 iLogtail 社区贡献，请阅读并同意遵守[阿里巴巴开源行为准则](https://github.com/alibaba/community/blob/master/CODE_OF_CONDUCT_zh.md)，共同营造一个开放透明且友好的开源社区环境。

## 贡献流程

我们随时欢迎任何贡献，无论是简单的错别字修正，BUG 修复还是增加新功能，包括但不限于代码、文档、案例等任何形式的社区互动。我们也同样重视与其它开源项目的结合，欢迎在这方面做出贡献。

### 如何入手？

如果您是初次贡献，可以先从 [good first issue](https://github.com/alibaba/loongcollector/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22) 列表中认领一个相对小的任务来快速参与社区贡献。

认领方式: 您可以直接在相应 Issue 中回复参与意愿，然后参照下面的 GitHub 工作流指引解决 issue 并按照规范提交 PR，通过 review 后即可合入到 main 分支。

### GitHub 完整工作流

我们使用 main 分支作为我们的开发分支，这代表它是不稳定的分支。稳定版本归档在 [Release](https://github.com/alibaba/loongcollector/releases) 中，每发布一个 Release 版本，都会打上对应版本 [Tag](https://github.com/alibaba/loongcollector/tags)。

#### 创建/认领 Issue

我们使用 GitHub [Issues](https://github.com/alibaba/loongcollector/issues) 以及 [Pull Requests](https://github.com/alibaba/loongcollector/pulls) 来管理/追踪需求或者问题。如果您希望开发新的特性、功能完善，或者发现了代码的 BUG，或者文档的增加、完善，或者希望提建议等，都可以创建一个 Issue；也可以认领我们发布出来的一些 Issue。

新建的 Issue 请按照模板规范填写内容，以便我们更好地理解您需求。另外，也需要您打上对应[标签](#标签)，方便我们进行分类管理。

#### 设计交流

如果您的 PR 包含非常大的变更(比如模块的重构或者添加新的组件)，**请务必发起详细讨论，编写详细的文档来阐述其设计、解决的问题和用途，达成一致后再进行变更**。您可以在 Issue 或 [Discussions ideas](https://github.com/alibaba/loongcollector/discussions/categories/ideas) 中给出详细的设计。如果需要我们的支持可以扫描加入钉钉群，我们可以提供一对一的交流。

对于一些小的变更，也请在 Issue 中简单描述您的设计。

#### 开发测试

设计定稿后，即可进行开发流程。下面是开源贡献者常用的工作流（workflow）：

1. 将 [iLogtail](https://github.com/alibaba/loongcollector) 仓库 fork 到个人 GitHub 下。
2. 基于个人 fork 分支进行开发、测试工作。详细流程：
    1. 保持个人 main 分支跟 iLogtail 主仓库 main 分支及时同步。
    2. 将 fork 后的个人仓库 clone 到本地。
    3. 创建新的开发分支，并进行开发。**请确保对应的变更都有 UnitTest 或 E2E 测试**。
    4. 在本地提交变更。**注意 commit log 保持简练、规范，提交的 email 需要和 GitHub 的 email 保持一致。**
    5. 将变更 push 到远程个人分支。
3. 向 iLogtail main 分支创建一个 [pull request (PR)](https://github.com/alibaba/loongcollector/pulls)，在进行较大的变更的时候请确保 PR 有一个对应的 Issue，并进行关联。

    1. 发起 PR 前请进行如下规范性检查：[代码/文档风格](../developer-guide/codestyle.md)、[编码规范](../developer-guide/code-check/check-codestyle.md)、[依赖包许可证](../developer-guide/code-check/check-dependency-license.md)、[文件许可证](../developer-guide/code-check/check-license.md)。
    2. 为了更好的进行版本管理，对于一些独立的特性或者关键BUG修复，请提交[Changelog](https://github.com/alibaba/loongcollector/blob/main/CHANGELOG.md).

注意一个 PR 尽量不要过于大，如果的确需要有大的变更，可以将其按功能拆分成多个单独的 PR。

若您是初次提交 PR，请先签署 CLA（PR 页面会有自动回复指引）。在提交 PR 后，系统会运行持续集成，请确保所有的 CI 均为 pass 状态。一切就绪后，我们会为 PR 分配一个或多个 reviewer，review通过后即可合入 main。

在合并 PR 的时候，请把多余的提交记录都 squash 成一个，保证最终的提交信息的简练、规范。

#### 标签

我们使用标签来进行 Issue、PR、Discussion 的管理。您可以根据实际情况打上对应的标签。

* 全新特性，对应 label: feature request。
* 现有功能完善，对应 label: enhancement。
* 适合新手开发，对应 label: good first issue。
* Bug，对应 label: bug。
* 测试框架或测试用例，对应 label: test。
* [配置样例或K8s部署案例](#config)，对应 label: example config。
* 文档（document），对应 label: documentation。
* [案例类](#case)，对应 label: awesome ilogtail。
* 回答、解决问题，对应 label: question。我们建议优先提到 [Discussions](https://github.com/alibaba/loongcollector/discussions) 中讨论。

以下为附加标签：

* 希望社区参与贡献，对应 label: community。
* 核心功能开发或变更，对应 label: core。

#### Code review

所有的代码都需要经过 committer 进行 review。以下是我们推荐的一些原则：

* 可读性：所有提交请遵循我们的[代码规范](../developer-guide/codestyle.md)、[文档规范](../developer-guide/plugin-development/plugin-docs/plugin-doc-templete.md)。
* 优雅性：代码简练、复用度高，有着完善的设计。
* 测试：重要的代码需要有完善的测试用例（单元测试、E2E 测试），对应的衡量标准是测试覆盖率。

### 配置分享 <a href="#config" id="config"></a>

除了共享代码，您也可以为我们的配置样板库做出贡献。

`example_config/data_pipelines`目录包含场景化的采集配置模版，重点突出processor / aggregator插件功能。如您对某些常见日志（如Apache、Spring Boot）的处理有一定心得，可以参考已有样例和[README](https://github.com/alibaba/loongcollector/tree/main/example_config#readme)进行提交。

`k8s_templates`目录包含完整的K8s部署案例，从源到典型处理到输出，以input / flusher插件进行划分。如您对某些常见的日志处理场景（如将数据采集到Elastic Search、Clickhouse）的部署有一定心得，可以参考已有样例和[README](https://github.com/alibaba/loongcollector/tree/main/k8s_templates#readme)进行提交。

### 案例分享 <a href="#case" id="case"></a>

我们也欢迎您分享任何关于 iLogtail 的使用案例。我们在知乎建立了专栏 [iLogtail社区](https://www.zhihu.com/column/c_1533139823409270785)，欢迎大家投稿，分享 iLogtail 的使用案例。

1. 在知乎写文章，例如[一文搞懂 SAE 日志采集架构](https://zhuanlan.zhihu.com/p/557591446)。
2. 推荐自己的文章到“iLogtail社区”专栏。
3. GitHub上修改[use-cases.md](https://github.com/alibaba/loongcollector/blob/main/docs/cn/awesome-ilogtail/use-cases.md)并发起PR，Label选awesome ilogtail。

### 参与社区讨论

如果您在使用 iLogtail 中遇到任何问题，欢迎到 [Discussions](https://github.com/alibaba/loongcollector/discussions) 进行交流互动。也欢迎在这里帮助其他使用者解答一些使用中的问题。

Discussion 分类：

* Announcements：iLogtail官方公告。
* Help：使用 iLogtail 中遇到问题，想在社区寻求帮助。
* Ideas：关于 iLogtail 的一些想法，欢迎随时交流。
* Show and tell：可以在这里展示任何跟 iLogtail 相关的工作，例如一些小工具等。

## 联系我们

<img src="https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/chatgroup/chatgroup.png" style="width: 60%; height: 60%" />
