# 检查依赖包许可证

iLogtail 基于Apache 2.0 协议进行开源，开发者需要保证依赖包协议与Apache 2.0 协议兼容，所有依赖包或源码引入License 说明位于根目录 `licenses` 文件夹。

## 检查依赖包License

开发者可以使用以下命令进行依赖包License 扫描。

```makefile
make check-dependency-licenses
```

- 当提示 `DEPENDENCIES CHECKING IS PASSED` 时，说明依赖包检查通过。
- 当提示 `FOLLOWING DEPENDENCIES SHOULD BE ADDED to LICENSE_OF_ILOGTAIL_DEPENDENCIES.md` 时，说明依赖包将需要被添加（需要保证协议与Apache 2.0 协议兼容），可以查看 `find_licenses/LICENSE-{liencese type}`文件查看待添加依赖包协议。
- 当提示 `FOLLOWING DEPENDENCIES IN LICENSE_OF_ILOGTAIL_DEPENDENCIES.md SHOULD BE REMOVED` 时，说明存在多余的依赖包，按提示进行删除即可。

## Fork 代码库管理

出于某些特性不支持原因，或精简依赖包原因，iLogtail 会存在某些Fork代码库，所有Fork代码库存在于[iLogtail](https://github.com/iLogtail)组织进行管理，出于License风险问题，禁止引入私人Fork版本。

### go.mod 管理

1. Fork 仓库： 对于Fork代码库，出于尊重原作者，禁止修改go.mod 仓库module地址，如[样例](https://github.com/iLogtail/go-mysql/blob/master/go.mod)所示。
2. iLogtail仓库: iLogtail 仓库对于Fork代码库要求使用replace 方式引入，用以保持代码文件声明的引入包地址保持原作者仓库地址。

```go
require (
    github.com/VictoriaMetrics/metrics v1.23.0
)

replace (
    github.com/VictoriaMetrics/metrics => github.com/iLogtail/metrics v1.23.0-ilogtail
)
```

### License 声明

请执行`make check-dependency-licenses` 指令，脚本程序将自动在find_licenses文件夹生成markdown 说明，请将说明放置于[LICENSE_OF_ILOGTAIL_DEPENDENCIES.md](../../../../licenses/LICENSE_OF_ILOGTAIL_DEPENDENCIES.md)文件末端，如下样例。

```go
## iLogtail used or modified source code from these projects
- [github.com/iLogtail/VictoriaMetrics fork from github.com/VictoriaMetrics/VictoriaMetrics](http://github.com/iLogtail/VictoriaMetrics) based on Apache-2.0
- [github.com/iLogtail/metrics fork from github.com/VictoriaMetrics/metrics](http://github.com/iLogtail/metrics) based on MIT
```

### 建议

如Fork 特性为原代码库的能力补充，非特定场景如精简依赖包等因素，建议对原始代码库提出PullRequest, 如原始仓库接受此次PullRequest，请将iLogtail 仓库依赖包地址修改为原始仓库地址，并删除Fork仓库。
