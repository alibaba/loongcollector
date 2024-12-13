# 聚合

聚合插件用于将事件进行打包，从而提升发送效率。仅限以下场景使用：

* 使用了除[SLS输出插件](../flusher/native/flusher-sls.md)

* 使用了[SLS输出插件](../flusher/native/flusher-sls.md)，且同时使用了[扩展处理插件](../processor/processors.md)

对于上述场景，如果用户的采集配置中未指定聚合插件，则`iLogtail`会使用默认聚合插件，即[上下文聚合插件](aggregator-context.md)。
