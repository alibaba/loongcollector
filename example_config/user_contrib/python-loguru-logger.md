# Python loguru.logger日志

## 提供者
[`cangkui`](https://github.com/cangkui)

## 描述
在许多python应用程序当中，开发者使用`loguru.logger`记录日志。比如，某Flask Web应用被Supervisor所部署，Supervisor配置文件如下：

```
[program:flask_gunicorn]
directory=/workspace/indicator_assess_system/
environment=FLASK_CONFIG='development',PYTHONUNBUFFERED=1
command=python3 ./run_flask.py
user=root
autorestart=true
startsecs=3
startretires=3
redirect_stderr=true
stopsignal=HUP
killasgroup=true
stopasgroup=true
stdout_logfile_maxbytes=100MB
stdout_logfile_backups = 20
stdout_logfile=/var/log/supervisor/flask_gunicorn.log
```

该配置文件将应用的标准输出重定向至`/var/log/supervisor/flask_gunicorn.log`当中，在应用当中使用`loguru.logger`产生的日志（如`loguru.logger.info`、`loguru.logger.error`等）也将输出至`flask_gunicorn.log`文件当中。

## 日志输入样例
```
2023-07-05 14:39:22.086 | ERROR    | flask_app.api:decorator:16 - the JSON object must be str, bytes or bytearray, not NoneType
2023-07-06 16:47:54.605 | INFO     | flask_app.service.influxdb_service:query_by_func:120 - QUERY SQL: 
            SELECT SUM("score") AS "score" FROM test_data 
            WHERE
                time>=$st AND time<=$ed
            GROUP BY
                *
            fill(previous)
2023-07-06 15:57:24.599 | ERROR    | flask_app.api:decorator:16 - (pymysql.err.OperationalError) (1049, "Unknown database 'indicator_assess'")
(Background on this error at: https://sqlalche.me/e/14/e3q8)
2023-07-07 11:32:31.863 | WARNING  | flask_app.api.inter_variable:fill_data_process:575 - point: N3DCS.CNDPINC "yc('N3DCS.3CNDP1')" exec error! e: name 'yc' is not defined
```

## 日志输出样例
```
{
	"time": "2023-07-05 14:39:22.086",
	"level": "ERROR",
	"msg": "flask_app.api:decorator:16 - the JSON object must be str, bytes or bytearray, not NoneType",
	"__time__": "1688783301"
}

{
	"time": "2023-07-06 16:47:54.605",
	"level": "INFO",
	"msg": "flask_app.service.influxdb_service:query_by_func:120 - QUERY SQL: \n            SELECT SUM(\"score\") AS \"score\" FROM test_data \n            WHERE\n                time>=$st AND time<=$ed\n            GROUP BY\n                *\n            fill(previous)",
	"__time__": "1688783301"
} 

{
	"time": "2023-07-06 15:57:24.599",
	"level": "ERROR",
	"msg": "flask_app.api:decorator:16 - (pymysql.err.OperationalError) (1049, \"Unknown database 'indicator_assess'\")\n(Background on this error at: https://sqlalche.me/e/14/e3q8)",
	"__time__": "1688783301"
} 

{
	"time": "2023-07-07 11:32:31.863",
	"level": "WARNING",
	"msg": "flask_app.api.inter_variable:fill_data_process:575 - point: N3DCS.CNDPINC \"yc('N3DCS.3CNDP1')\" exec error! e: name 'yc' is not defined",
	"__time__": "1688783301"
}
```

## 采集配置
```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: ./test-log
    FilePattern: reg.log
processors:
  - Type: processor_split_log_regex
    SplitKey: content
    SplitRegex: '\d+-\d+-\d+.*'
  - Type: processor_regex
    SourceKey: content
    Regex: '(\d+-\d+-\d+\s\d+:\d+:\d+\.\d+)\s+\|\s+(\S+)\s+\|\s+(.*)'
    Keys: ["time", "level", "msg"]
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```


备注：在iLogtail项目顶层目录下新建了`test-log`文件夹，并在其中创建了`reg.log`日志文件，该日志文件当中的内容即为输入样例的内容。
