# 开发环境

## 插件

iLogtail 插件基于 Go 语言实现，所以在进行开发前，需要安装基础的 Go 1.16+
语言开发环境，如何安装可以参见[官方文档](https://golang.org/doc/install)。
在安装完成后，为了方便后续地开发，请遵照[此文档](https://golang.org/doc/code#Organization)
正确地设置你的开发目录以及 GOPATH 等环境变量。

## Core

iLogtail Core使用C++ 语言实现，当前使用GCC 4.8.x，使用其他版本可能无法兼容。

## VSCode环境

推荐使用VSCode作为iLogtail的开发环境。

必备插件
[Go](https://marketplace.visualstudio.com/items?itemName=golang.Go)
[C/C++ Excention Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack)
[Clang-Format](https://marketplace.visualstudio.com/items?itemName=xaver.clang-format)

推荐插件
[GitLens](https://marketplace.visualstudio.com/items?itemName=eamodio.gitlens)
[Doxygen Documentation Generator](https://marketplace.visualstudio.com/items?itemName=cschlosser.doxdocgen)
[Markdown Preview Enhanced](https://marketplace.visualstudio.com/items?itemName=shd101wyy.markdown-preview-enhanced)
[markdownlint](https://marketplace.visualstudio.com/items?itemName=DavidAnson.vscode-markdownlint)
[YAML](https://marketplace.visualstudio.com/items?itemName=redhat.vscode-yaml)
[CMake Language Support](https://marketplace.visualstudio.com/items?itemName=josetr.cmake-language-support-vscode)
