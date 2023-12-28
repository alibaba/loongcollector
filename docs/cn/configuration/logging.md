# 日志

完整的iLogtail由`C++`开发的主程序及`Golang`开发的插件组成，因此iLogtail的运行日志也有两部分组成。

## iLogtail主程序日志

日志控制文件：`apsara_log_conf.json，`该文件`iLogtail`首次运行时会自动生成。

日志级别：`TRACE`、`DEBUG`、`INFO`、`WARNING`、`ERROR`、`FATAL`

日志类型

* 运行日志：
  * 日志文件：`ilogtail.LOG`
  * 日志级别配置项：`Loggers:/apsara/sls/ilogtail:AsyncFileSink`
* `profile`日志：
  * 日志文件：`snapshot/ilogtail_profile.LOG`
  * 日志级别配置项: 不需要修改。
* `status`日志
  * 日志文件：`snapshot/ilogtail_status.LOG`
  * 日志级别配置项: 不需要修改。

完整配置项：

```json
{
 "Loggers" :
 {
  "/" :
  {
   "AsyncFileSink" : "WARNING"
  },
  "/apsara/sls/ilogtail" :
  {
   "AsyncFileSink" : "INFO"
  },
  "/apsara/sls/ilogtail/profile" :
  {
   "AsyncFileSinkProfile" : "INFO"
  },
  "/apsara/sls/ilogtail/status" :
  {
   "AsyncFileSinkStatus" : "INFO"
  }
 },
 "Sinks" :
 {
  "AsyncFileSink" :
  {
   "Compress" : "Gzip",
   "LogFilePath" : "${ilogtail运行路径}/ilogtail.LOG",
   "MaxDaysFromModify" : 300,
   "MaxLogFileNum" : 10,
   "MaxLogFileSize" : 20000000,
   "Type" : "AsyncFile"
  },
  "AsyncFileSinkProfile" :
  {
   "Compress" : "",
   "LogFilePath" : "${ilogtail运行路径}/snapshot/ilogtail_profile.LOG",
   "MaxDaysFromModify" : 1,
   "MaxLogFileNum" : 61,
   "MaxLogFileSize" : 1,
   "Type" : "AsyncFile"
  },
  "AsyncFileSinkStatus" :
  {
   "Compress" : "",
   "LogFilePath" : "${ilogtail运行路径}/snapshot/ilogtail_status.LOG",
   "MaxDaysFromModify" : 1,
   "MaxLogFileNum" : 61,
   "MaxLogFileSize" : 1,
   "Type" : "AsyncFile"
  }
 }
}
```

## iLogtail插件日志

日志文件：`logtail_plugin.LOG`

日志控制文件：`plugin_logger.xml`

日志级别：`trace`、`debug`、`info`、`warn`、`error`、`critical`

日志级别配置项：修改`minlevel`字段取值即可

完整配置文件：

```xml
<seelog type="asynctimer" asyncinterval="500000" minlevel="info" >
 <outputs formatid="common">
  <rollingfile type="size" filename="${ilogtail运行路径}/logtail_plugin.LOG" maxsize="2097152" maxrolls="10"/>


 </outputs>
 <formats>
  <format id="common" format="%Date %Time [%LEV] [%File:%Line] [%FuncShort] %Msg%n" />
 </formats>
</seelog>
```
