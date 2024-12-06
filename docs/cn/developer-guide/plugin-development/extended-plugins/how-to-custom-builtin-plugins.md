# 如何自定义构建产物中默认包含的插件

## 插件引用机制

iLogtail 通过 [插件引用配置文件](https://github.com/alibaba/loongcollector/blob/main/plugins.yml) 来定义要包含在构建产物中的插件，该文件中默认包含了iLogtail仓库中的所有插件。

同时，iLogtail 也以同样的机制支持引入外部私有插件，关于如何开发外部插件，请参阅[如何构建外部私有插件](how-to-write-external-plugins.md)。iLogtail 默认会检测仓库根目录下的 `external_plugins.yml` 文件来查找外部插件定义。

当执行诸如 `make all` 等构建指令时，该配置文件会被解析并生成 go import 文件到 [plugins/all](https://github.com/alibaba/loongcollector/tree/main/plugins/all) 目录下。

插件引用配置文件的格式定义如下：

```yaml
plugins:    // 需要注册的plugins，按适用的系统分类
common:
  - gomod: code.private.org/private/custom_plugins v1.0.0  // 可选，插件module，仅针对外部插件
    import: code.private.org/private/custom_plugins        // 可选，代码中import的package路径
    path: ../custom_plugins                                // 可选，replace 本地路径，用于调试
windows:
  
linux:

project:
  replaces:       // 可选，array，用于解决多个插件module之间依赖冲突时的问题
  go_envs:        // 可选，map，插件的repo是私有的时候，可以添加如GOPRIVATE环境等设置
    GOPRIVATE: "*.code.org"
  git_configs:    // 可选，map，私有插件repo可能需要认证，可以通过设置git url insteadof调整
    url.https://user:token@github.com/user/.insteadof: https://github.com/user/
```

## 自定义构建产物中包含的插件

### 方式一：修改默认的 `plugins.yml` 或 `external_plugins` 文件

如前文所述，您可以通过直接修改默认的 [插件引用配置文件](https://github.com/alibaba/loongcollector/blob/main/plugins.yml) 文件内容，来选择要包含在构建产物中的插件。

### 方式二：自定义文件

您也可以通过指定一个`自定义的插件引用配置文件`来指导构建，该文件可以是一个本地文件或者远程文件url，该文件的内容格式应当与默认的 plugins.yml文件一致。

假设您自定义的插件引用配置文件名为 `/tmp/custom_plugins.yml`，可以通过设置 `PLUGINS_CONFIG_FILE` 环境变量为该文件的路径来指导构建，如：

```shell
PLUGINS_CONFIG_FILE=/tmp/custom_plugins.yml make all
```

注：`PLUGINS_CONFIG_FILE` 支持多个配置文件路径，用英文逗号分割。
