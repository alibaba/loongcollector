# PostgreSQL 导入插件

## 简介

`service_pgsql` `input`插件可以采集PostgreSQL 查询数据。

## 版本

[Beta](../stability-level.md)

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| --- | --- | --- |
| Type | String，无默认值（必填） | 插件类型，指定为`service_pgsql`。 |
| Host| String，`127.0.0.1` | 数据库主机。|
| Port | Interger，`5432` | 数据库端口。|
| User | String，`root` | 数据库用户名。|
| Password | String，默认值为空 | 数据库密码。|
| Database | String，默认值为空 | 数据库名称。|
| DialTimeOutMs | Interger，5000 | 数据库连接超时时间，单位：ms，默认5000ms |
| Statement | String，默认值为空| SQL语句。设置CheckPoint为true时，StateMent中SQL语句的where条件中必须包含CheckPointColumn，并将该列的值配置为$1。例如：CheckPointColumn配置为id，则StateMent配置为SELECT * from ... where id > $1。 |
| Limit | Boolean，`false`| 是否使用Limit分页。不配置时，默认为false，表示不使用Limit分页。建议使用Limit进行分页。设置Limit为true后，进行SQL查询时，会自动在StateMent中追加LIMIT语句。 |
| PageSize | Interger，无默认值 | 分页大小，Limit为true时必须配置。|
| MaxSyncSize | Interger，`0` | 每次同步最大记录数。不配置时，默认为0，表示无限制。|
| CheckPoint | Boolean，`false`| 是否使用checkpoint。不配置时，默认为false，表示不使用checkpoint。|
| CheckPointColumn | String，无默认值| checkpoint列名称。 CheckPoint为true时必须配置。 注意 该列的值必须递增，否则可能会出现数据漏采集问题（每次查询结果中的最大值将作为下次查询的输入）。|
| CheckPointColumnType | String，无默认值| checkpoint列类型，支持int和time两种类型。int类型的内部存储为int64，time类型支持MySQL的date、datetime、time。 CheckPoint为true时必须配置。|
| CheckPointStart | String，无默认值| checkpoint初始值。CheckPoint为true时必须配置。|
| CheckPointSavePerPage | Boolean，无默认值| 设置为true，则每次分页时保存一次checkpoint；设置为false，则每次同步完后保存checkpoint。|
| IntervalMs | Interger，无默认值| 同步间隔，单位：ms。|

## 样例

对postgres数据库下的specialalarmtest进行读取。

* 表结构及数据信息如下

```sql
CREATE TABLE IF NOT EXISTS specialalarmtest (
    id BIGSERIAL NOT NULL, 
 time TIMESTAMP NOT NULL, 
 alarmtype varchar(64) NOT NULL, 
 ip varchar(16) NOT NULL, 
 COUNT INT NOT NULL, 
 PRIMARY KEY (id)
);

insert into specialalarmtest (time, alarmtype, ip, count) values(now(), 'NO_ALARM', '10.10.***.***', 0);
insert into specialalarmtest (time, alarmtype, ip, count) values(now(), 'NO_ALARM', '10.10.***.***', 1);
insert into specialalarmtest (time, alarmtype, ip, count) values(now(), 'NO_ALARM', '10.10.***.***', 2);
insert into specialalarmtest (time, alarmtype, ip, count) values(now(), 'NO_ALARM', '10.10.***.***', 3);
```

* 采集配置

```yaml
enable: true
inputs:
    - Type: service_pgsql
      Address: 127.0.0.1
      CheckPoint: true
      CheckPointColumn: id
      CheckPointColumnType: int
      CheckPointSavePerPage: true
      CheckPointStart: "0"
      Database: postgres
      IntervalMs: 1000
      Limit: true
      MaxSyncSize: 100
      PageSize: 100
      User: postgres
      Password: xxxxx 
      StateMent: "select * from specialalarmtest where id > $1"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{"id":"1","time":"2022-08-25T06:01:07.276901Z","alarmtype":"NO_ALARM","ip":"10.10.***.***","count":"0","__time__":"1661416452"}
{"id":"2","time":"2022-08-25T06:01:07.277624Z","alarmtype":"NO_ALARM","ip":"10.10.***.***","count":"1","__time__":"1661416452"}
{"id":"3","time":"2022-08-25T06:01:07.278072Z","alarmtype":"NO_ALARM","ip":"10.10.***.***","count":"2","__time__":"1661416452"}
{"id":"4","time":"2022-08-25T06:01:07.278494Z","alarmtype":"NO_ALARM","ip":"10.10.***.***","count":"3","__time__":"1661416452"}
```
