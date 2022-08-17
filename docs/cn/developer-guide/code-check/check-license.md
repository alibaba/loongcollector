# 检查文件许可证

LogtailPlugin 遵循标准的 Apache 2.0 许可，所有文件必须具有许可证声明。 所以贡献者应该检查贡献的文件是否拥有此许可证。

## 检查许可证

如果要检查整个项目文件，请运行以下命令。 没有许可证的文件将被找到并保存为 license_coverage.txt。

```makefile
   make check-license
```

当需要固定范围时，请在命令中附加`SCOPE`。

```makefile
SCOPE=xxx make check-license
```

xxx为限定的目录名称。

## 添加许可证

当某些带有许可证的文件建立后，您可以通过以下方式将许可证添加到文件中。 请注意，这种方式将修复所有缺少许可证的文件。

```makefile
     make license
```

同上，如果你想限制变化范围，请在命令中附加`SCOPE`。

```makefile
SCOPE=xxx make license
```

xxx为限定的目录名称。
