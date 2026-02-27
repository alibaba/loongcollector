# 环境说明

本目录所含的 devcontainer 是编译和测试 LoongCollector 的开发环境。

为了适配大部分运行环境，包括陈旧的 Linux 版本，LoongCollector 的编译环境只能使用 CentOS 7。但当前先进的开发工具，如 VSCode、Cursor、Claude Code、Gemini Cli 均已逐步放弃对旧系统的支持，因此开发模式转变为旧系统编译，新系统开发。

- 开发时：新系统和旧系统共享代码仓库，新系统中的修改在老系统中实时感知。
- 编译、测试时：新系统中 ssh 到老系统进行编译，新系统接收老系统执行结果作为反馈。

为了避免端口冲突，devcontainer.json 中没有添加端口映射，如需使用请自行在`"runArgs"`中添加，如`"-p", "2322:22"`。
