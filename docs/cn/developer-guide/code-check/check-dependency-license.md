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
