# 单条协议

单条协议在内存中对应的数据结构为`map[string]interface{}`，其中的键及相应值的类型为：

- time：`uint32`类型
- contents：`map[string]string`类型
- tags：`map[string]string`类型
