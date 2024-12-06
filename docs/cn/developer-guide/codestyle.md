# 代码风格

iLogtail C++遵循基于[Google代码规范](https://google.github.io/styleguide/cppguide.html)的风格，详细格式约束见[.clang-format](https://github.com/alibaba/loongcollector/blob/main/.clang-format)。

Go遵循[Effective Go](https://go.dev/doc/effective_go)风格。

Markdown遵循基于[markdownlint](<https://github.com/DavidAnson/markdownlint/blob/main/doc/Rules.md>)的风格，并为了兼容[GitBook](https://docs.gitbook.com/editing-content/rich-content/with-command-palette#hints-and-callouts)排除[MD033](<https://github.com/DavidAnson/markdownlint/blob/main/doc/Rules.md#md033---inline-html>)。

## 格式化C++代码

使用VSCode的Clang-Format (by xaver)插件。

或使用命令行进行格式化

```bash
find core/ -type f -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i
```

## 格式化Go代码

使用VSCode的Go (by Go Team at Google)插件。

或使用命令行进行格式化

```bash
find ./ -type f -iname '*.go' -not -iname '*.pb.go' | grep -E -v '/external/' | xargs gofmt -w
```

## 格式化Markdown

使用VSCode的Markdownlint (by David Anson)插件。
