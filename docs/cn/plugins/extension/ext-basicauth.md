# BasicAuth认证扩展

## 简介

`ext_basicauth` 扩展插件，实现了 [extensions.ClientAuthenticator](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/extensions/authenticator.go) 接口，可以在 http_flusher 插件中引用，提供向请求中添加 basic auth Header 的能力。

## 版本

[Alpha](../stability-level.md)

## 配置参数

| 参数       | 类型     | 是否必选 | 说明  |
|----------|--------|------|-----|
| Username | String | 是    | 用户名 |
| Password | String | 是    | 密码  |

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果以 `custom_single` 协议、`json`格式提交到 `http://localhost:8086/write`。
且提交时，使用 basic auth 认证扩展。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_http
    RemoteURL: "http://localhost:8086/write"
    Convert:
      Protocol: custom_single
      Encoding: json
    Authenticator: 
      Type: ext_basicauth
extensions:
  - Type: ext_basicauth
    Username: user1
    Password: pwd1
```

## 使用命名扩展

当希望在同一个pipeline中使用多个扩展时，可一给同一类型的不同扩展实例附加一个命名，引用时可以通过full-name来引用特定的实例。

如下示例，通过将`Type: ext_basicauth` 改写为 `Type: ext_basicauth/user1`，追加命名为`user1`，在 http_flusher插件中通过 `ext_basicauth/user1`来引用。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_http
    RemoteURL: "http://localhost:8086/write"
    Convert:
      Protocol: custom_single
      Encoding: json
    Authenticator: 
      Type: ext_basicauth/user1
extensions:
  - Type: ext_basicauth/user1
    Username: user1
    Password: pwd1
  - Type: ext_basicauth/user2
    Username: user2
    Password: pwd2
```
