# 概览

## 输入

| 名称                                              | 提供方                                                       | 简介                                                                                 |
| ------------------------------------------------- | ------------------------------------------------------------ | ------------------------------------------------------------------------------------ |
| `file_log`<br> 文本日志                           | SLS官方<br>[`messixukejia`](https://github.com/messixukejia) | 文本采集。                                                                           |
| `input_docker_stdout`<br>容器标准输出             | SLS官方                                                      | 从容器标准输出/标准错误流中采集日志。                                                |
| `metric_debug_file`<br>文本日志（debug）          | SLS官方                                                      | 用于调试的读取文件内容的插件。                                                       |
| `metric_input_example`<br>MetricInput示例插件     | SLS官方                                                      | MetricInput示例插件。                                                                |
| `metric_meta_host`<br>主机Meta数据                | SLS官方                                                      | 主机Meta数据。                                                                       |
| `metric_mock`<br>Mock数据-Metric                  | SLS官方                                                      | 生成metric模拟数据的插件。                                                           |
| `service_canal`<br>MySQL Binlog                   | SLS官方                                                      | 将MySQL Binlog输入到iLogtail。                                                       |
| `service_input_example`<br>ServiceInput示例插件   | SLS官方                                                      | ServiceInput示例插件。                                                               |
| `service_journal`<br>Journal数据                  | SLS官方                                                      | 从原始的二进制文件中采集Linux系统的Journal（systemd）日志。                          |
| `service_kafka`<br>Kafka                          | SLS官方                                                      | 将Kafka数据输入到iLogtail。                                                          |
| `service_mock`<br>Mock数据-Service                | SLS官方                                                      | 生成service模拟数据的插件。                                                          |
| `service_mssql`<br>SqlServer查询数据              | SLS官方                                                      | 将Sql Server数据输入到iLogtail。                                                     |
| `service_pgsql`<br>PostgreSQL查询数据             | SLS官方                                                      | 将PostgresSQL数据输入到iLogtail。                                                    |
| `service_syslog`<br>Syslog数据                    | SLS官方                                                      | 采集syslog数据。                                                                     |
| `observer_ilogtail_network`<br>无侵入网络调用数据 | SLS官方                                                      | 支持从网络系统调用中收集四层网络调用，并借助网络解析模块，可以观测七层网络调用细节。 |

## 处理

| 名称                                               | 提供方                                              | 简介                                             |
| -------------------------------------------------- | --------------------------------------------------- | ------------------------------------------------ |
| `processor_add_fields`<br>添加字段                 | SLS官方                                             | 添加字段。                                       |
| `processor_default`<br>原始数据                    | SLS官方                                             | 不对数据任何操作，只是简单的数据透传。           |
| `processor_drop`<br>丢弃字段                       | SLS官方                                             | 丢弃字段。                                       |
| `processor_fields_with_conditions`<br>条件字段处理 | 社区<br>[`pj1987111`](https://github.com/pj1987111) | 根据日志部分字段的取值，动态进行字段扩展或删除。 |
| `processor_filter_regex`<br>日志过滤               | SLS官方                                             | 通过正则匹配过滤日志。                           |
| `processor_json`<br>Json                           | SLS官方                                             | 实现对Json格式日志的解析。                       |
| `processor_regex`<br>正则                          | SLS官方                                             | 通过正则匹配的模式实现文本日志的字段提取。       |
| `processor_rename`<br>重命名字段                   | SLS官方                                             | 重命名字段。                                     |
| `processor_split_char`<br>分隔符                   | SLS官方                                             | 通过单字符的分隔符提取字段。                     |
| `processor_split_key_value`<br>键值对              | SLS官方                                             | 通过切分键值对的方式提取字段。                   |
| `processor_split_log_regex`<br>多行切分            | SLS官方                                             | 实现多行日志（例如Java程序日志）的采集。         |
| `processor_split_string`<br>分隔符                 | SLS官方                                             | 通过多字符的分隔符提取字段。                     |

## 聚合

## 输出

| 名称                              | 提供方  | 简介                                 |
| --------------------------------- | ------- | ------------------------------------ |
| `flusher_kafka`<br>Kafka          | 社区    | 将采集到的数据输出到Kafka。          |
| `flusher_sls`<br>SLS              | SLS官方 | 将采集到的数据输出到SLS。            |
| `flusher_stdout`<br>标准输出/文件 | SLS官方 | 将采集到的数据输出到标准输出或文件。 |

## 加速

| 名称                                           | 提供方  | 简介                                           |
| ---------------------------------------------- | ------- | ---------------------------------------------- |
| `processor_delimiter_accelerate`<br>分隔符加速 | SLS官方 | 以加速模式实现分隔符日志的字段提取。           |
| `processor_json_accelerate`<br>Json加速        | SLS官方 | 以加速模式实现`Json`格式日志的字段提取。       |
| `processor_regex_accelerate`<br>正则加速       | SLS官方 | 通过正则匹配以加速模式实现文本日志的字段提取。 |
