# Canal

## 简介

`service_canal` `input`插件可以采集MySQL Binlog。

## 版本

[Beta](../stability-level.md)

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| Type | String，无默认值（必填） | 插件类型，指定为`service_canal`。 |
| Host| String，`127.0.0.1` | 数据库主机。|
| Port | Interger，`3306` | 数据库端口。|
| User | String，`root` | 数据库用户名。|
| Password | String，默认值为空 | 数据库密码。|
| ServerID | Interger，125 | Logtail模拟Mysql Slave的ID。|
| IncludeTables | Array， 无默认值（必填） | 包含的表名称（包括db，例如：test_db.test_table），可配置为正则表达式。如果某个表不符合IncludeTables中的任一条件则该表不会被采集。如果您希望采集所有表，请将此参数设置为`.*\\..*`。如果需要完全匹配，请在前面加上^，后面加上$，例如：^test_db\\.test_table$。 |
| ExcludeTables | Array， 无默认值（必填） | 忽略的表名称（包括db，例如：test_db.test_table），可配置为正则表达式。如果某个表符合ExcludeTables中的任一条件则该表不会被采集。不设置时默认收集所有表。|
| StartBinName | String，`""` | 首次采集的Binlog文件名。不设置时，默认从当前时间点开始采集。如果想从指定位置开始采集，可以查看当前的Binlog文件以及文件大小偏移量，并将StartBinName、StartBinlogPos设置成对应的值。 |
| StartBinlogPos | Interger，`true` | 是否附加全局事务ID。不设置时，默认为true。设置为false时，表示上传的数据不附加全局事务ID。|
| EnableGTID | Boolean，`true`  | 是否附加全局事务ID。不设置时，默认为true。设置为false时，表示上传的数据不附加全局事务ID。|
| EnableInsert | Boolean，`true` | 是否采集insert事件的数据。不设置时，默认为true。设置为false时，表示将不采集insert事件数据。|
| EnableUpdate | Boolean，`true` | 是否采集update事件的数据。不设置时，默认为true。设置为false时，表示不采集update事件数据。 |
| EnableDelete | Boolean，`true` | 是否采集delete事件的数据。不设置时，默认为true。设置为false时，表示不采集delete事件数据。|
| EnableDDL | Boolean，`false` | 是否采集DDL（data definition language）事件数据。不设置时， 默认为false，表示不收集DDL事件数据。该选项不支持IncludeTables和ExcludeTables过滤。|
| Charset | String，`utf-8` | 编码方式。不设置时，默认为utf-8。|
| TextToString | Boolean，`false` | 是否将text类型的数据转换成字符串。不设置时，默认为false，表示不转换。|
| PackValues | Boolean，`false` | 是否将事件数据打包成JSON格式。默认为false，表示不打包。如果设置为true，Logtail会将事件数据以JSON格式集中打包到data和old_data两个字段中，其中old_data仅在row_update事件中有意义。 示例：假设数据表有三列数据c1，c2，c3，设置为false，row_insert事件数据中会有c1，c2，c3三个字段，而设置为true时，c1，c2，c3会被统一打包为data字段，值为`{"c1":"...", "c2": "...", "c3": "..."}`。|
| EnableEventMeta | Boolean，`false` | 是否采集事件的元数据，默认为false，表示不采集。 Binlog事件的元数据包括event_time、event_log_position、event_size和event_server_id。|
| UseDecimal | Boolean，`false` | Binlog解析DECIMAL类型时，是否保持原格式输出，而不是使用科学计数法。如果未设置，系统默认为false，即默认使用科学计数法。|

## 样例

对user_info数据库下的SpecialAlarm表分别执行INSERT、UPDATE、DELETE操作，数据库表结构、数据库操作及日志样例如下所示。

* 表结构

```sql
CREATE TABLE `titles` (
  `emp_no` int(11) NOT NULL,
  `title` varchar(50) NOT NULL,
  `from_date` date NOT NULL,
  `to_date` date DEFAULT NULL,
  PRIMARY KEY (`emp_no`,`title`,`from_date`),
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
```

* 表内容，其中一行

```text
emp_no title from_date to_date
10006 Senior Engineer 1990-08-05 9999-01-01
```

* 执行三条语句

```sql
update titles set title = 'test-update' where emp_no = 10006

delete from titles where emp_no = 10006

INSERT INTO `titles` (`emp_no`, `title`, `from_date`, `to_date`)
VALUES
 (10006, 'Senior Engineer', '1990-08-05', '9999-01-01'); 
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: service_canal
    Host: 127.0.0.1
    Port: 3306
    ServerID: 123456
    Password: xxxxx
    EnableDDL: true
    TextToString: true
flushers:
  - Type: flusher_kafka
    Brokers:
      - localhost:9092
    Topic: access-log
```

* 输出

```json
{"Time":1657890330,"Contents":[{"Key":"_table_","Value":"titles"},{"Key":"_offset_","Value":"4308"},{"Key":"_old_emp_no","Value":"10006"},{"Key":"from_date","Value":"1990-08-05"},{"Key":"_host_","Value":"127.0.0.1"},{"Key":"_event_","Value":"row_update"},{"Key":"_id_","Value":"12"},{"Key":"_old_from_date","Value":"1990-08-05"},{"Key":"_gtid_","Value":"00000000-0000-0000-0000-000000000000:0"},{"Key":"_db_","Value":"employees"},{"Key":"_filename_","Value":"mysql-bin.000001"},{"Key":"_old_title","Value":"Senior Engineer"},{"Key":"_old_to_date","Value":"9999-01-01"},{"Key":"emp_no","Value":"10006"},{"Key":"title","Value":"test-update"},{"Key":"to_date","Value":"9999-01-01"}]}

{"Time":1657890333,"Contents":[{"Key":"_id_","Value":"13"},{"Key":"_filename_","Value":"mysql-bin.000001"},{"Key":"emp_no","Value":"10006"},{"Key":"title","Value":"test-update"},{"Key":"_db_","Value":"employees"},{"Key":"_table_","Value":"titles"},{"Key":"_event_","Value":"row_delete"},{"Key":"from_date","Value":"1990-08-05"},{"Key":"to_date","Value":"9999-01-01"},{"Key":"_host_","Value":"127.0.0.1"},{"Key":"_gtid_","Value":"00000000-0000-0000-0000-000000000000:0"},{"Key":"_offset_","Value":"4660"}]}

{"Time":1657890335,"Contents":[{"Key":"_offset_","Value":"4975"},{"Key":"emp_no","Value":"10006"},{"Key":"title","Value":"Senior Engineer"},{"Key":"from_date","Value":"1990-08-05"},{"Key":"_gtid_","Value":"00000000-0000-0000-0000-000000000000:0"},{"Key":"_filename_","Value":"mysql-bin.000001"},{"Key":"_table_","Value":"titles"},{"Key":"_event_","Value":"row_insert"},{"Key":"_id_","Value":"14"},{"Key":"to_date","Value":"9999-01-01"},{"Key":"_host_","Value":"127.0.0.1"},{"Key":"_db_","Value":"employees"}]}
```
