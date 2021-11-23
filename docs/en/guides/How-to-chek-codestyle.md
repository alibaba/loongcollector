# How to check code style?

LogtailPlugin use [golangci-lint](https://golangci-lint.run/) to check the code style. The `SCOPE` definition is
optional, the default scope is root directory.

```makefile
SCOPE=xxx make lint
```
