# input_command 插件

## 简介

`input_command`插件可以通过配置脚本内容，在agent机器上生成可执行的脚本，并通过指定的`cmdpath` 执行该脚本，插件会从脚本执行后`stdout`获得内容进行解析，从而获取脚本内容执行后的信息。

注意：如果需要使用该插件，iLogtail需要在root用户下使用。配置的脚本内容请自查风险。

## 版本

[Alpha](../../stability-level.md)

## 配置参数

### 基础参数

| 参数                  | 类型       | 是否必选 | 说明                                                                                                                                                                             |
|---------------------|----------|------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Type                | String   | 是    | 插件类型，指定为`input_command`                                                                                                                                                        |
| ScriptType          | String   | 是    | 指定脚本内容的类型，目前支持:bash、shell、python2、python3                                                                                                                                      |
| User                | String   | 是    | 运行命令使用的用户名，只支持非Root用户(建议配置最小权限，只给需要关注的目录/文件rwx权限)                                                                                                                              |
| ScriptContent       | String   | 是    | 脚本内容, 支持PlainText和base64加密的内容, 跟ContentEncoding的字段对应, ScriptContent长度不能超过512*1024                                                                                              |
| ContentEncoding     | String   | 否    | 脚本内容的文本格式 <br/> 支持PlainText(纯文本，不编码)\|Base64编码 默认:PlainText                                                                                                                    |
| LineSplitSep        | String   | 否    | 脚本输出内容的分隔符，为空时不进行分割，全部作为一条数据返回                                                                                                                                                 |
| CmdPath             | String   | 否    | 执行脚本命令的路径，如果为空，则使用默认路径。bash、shell、python2、python3对应的默认路径如下：<br/>- bash: /usr/bin/bash<br/>- shell: /usr/bin/sh<br/>- python2: /usr/bin/python2<br/>- python3: /usr/bin/python3 |
| TimeoutMilliSeconds | int      | 否    | 执行脚本的超时时间，单位为毫秒，默认为3000ms                                                                                                                                                      |
| IntervalMs          | int      | 否    | 采集触发频率，也是脚本执行的频率，单位为毫秒，默认为5000ms                                                                                                                                               |
| Environments        | []string | 否    | 环境变量，默认为os.Environ()的值，如果设置了Environments，则在os.Environ()的基础上追加设置的环境变量                                                                                                           |
| IgnoreError         | Bool     | 否    | 插件执行出错时是否输出Error日志。如果未添加该参数，则默认使用false，表示不忽略                                                                                                                                   |

### 生成参数

| 参数         | 类型     | 说明                                             |
|------------|--------|------------------------------------------------|
| content    | String | 表示脚本的输出内容                                      |
| script_md5 | String | 用于表示 ScriptContent（脚本内容）的 MD5，有助于确定生成日志的脚本内容来源 |

* 采集配置1

```yaml
enable: true
inputs:
  - Type: input_command
    User: test
    ScriptType: shell
    ScriptContent:
      echo -e "test metric commond"
    Environments:
      - "DEBUG=true"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
  "content":"test metric commond",
  "script_md5":"b3ebc535c2e6cead6e2e13b383907245",
  "__time__":"1689677617"
}
```

* 采集配置2

```yaml
enable: true
inputs:
  - Type: input_command
    User: test
    ScriptType: python2
    ScriptContent: |
      print("test input_command 0")
      print("test input_command 1")
      print("test input_command 2")
      print("test input_command 3")
      print("test input_command 4")
      print("test input_command 5")
      print("test input_command 6")
      print("test input_command 7")
      print("test input_command 8")
      print("test input_command 9")
    CmdPath: /usr/bin/python
    Environments:
      - "DEBUG=true"
    TimeoutMilliseconds: 1005
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
  "content":"test input_command 0\ntest input_command 1\ntest input_command 2\ntest input_command 3\ntest input_command 4\ntest input_command 5\ntest input_command 6\ntest input_command 7\ntest input_command 8\ntest input_command 9",
  "script_md5":"5d049f5b2943d2d5e5737dddb065c39c",
  "__time__":"1689677656"
}
```

* 采集配置3

```yaml
enable: true
inputs:
  - Type: input_command
    User: test
    ScriptType: python2
    ScriptContent: |
      import os
      print os.environ
    CmdPath: /usr/bin/python
    Environments:
      - "DEBUG=true"
    TimeoutMilliseconds: 1005
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
  "content":"{'GOPATH': '/opt/go', 'GOROOT': '/usr/local/go',  'DEBUG': 'true', xxxxx(省略后面内容）}",
  "script_md5":"1444286e16118f7d1bb778738a828273",
  "__time__":"1689677681"
}
```
