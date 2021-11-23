# 如何检查代码风格？

LogtailPlugin 使用 [golangci-lint](https://golangci-lint.run/) 检查代码风格。 `SCOPE` 定义是
可选，默认范围是根目录。

```makefile
SCOPE=xxx make lint
```