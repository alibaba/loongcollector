# 系统参数

在`iLogtail`启动时，会加载`ilogtail_config.json`配置文件，该配置文件指定了`iLogtail`正常运行的一些基本配置项。

## 参数列表



| 参数                      | 类型     | 说明                                                                                                                                                                                                                                   |
| ----------------------- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `cpu_usage_limit`       | double | <p>CPU使用阈值，以单核计算。取值范围：0.1~当前机器的CPU核心数</p><p><strong></strong></p><p><strong>警告</strong> cpu_usage_limit为软限制，实际Logtail占用的CPU可能超过限制值，超限5分钟后将触发熔断保护，Logtail自动重启。</p><p>例如设置为0.4，表示日志服务将尽可能限制Logtail的CPU使用为CPU单核的40%，超出后Logtail自动重启。</p> |
| `mem_usage_limit`       | Int    | <p></p><p>内存使用阈值。</p><p><strong>警告</strong> <code>mem_usage_limit</code>为软限制，实际<code>iLogtail</code>占用的内存可能超过限制值，超限5分钟后将触发熔断保护，Logtail自动重启。</p>                                                                                      |
| `default_access_key_id` | String | 写入`SLS`的`access_id`，需要具备写入权限。                                                                                                                                                                                                                |
| `default_access_key`    | String | 写入`SLS`的`access_key`，需要具备写入权限。                                                                                                                                                                                                                 |

## 典型配置

```
{
    "default_access_key_id": "",
    "default_access_key": "",
    "cpu_usage_limit" : 0.4,
    "mem_usage_limit" : 384
}
```
