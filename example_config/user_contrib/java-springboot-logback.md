# Java Spring Boot Logback 日志

## 提供者

[`gohonsen`](https://github.com/gohonsen)

## 描述

Java Spring Boot 项目大多使用 Logback 日志框架写日志, Logback 配置比较灵活，可通过配置文件自定义配置日志路径、日志字段内容，有些字段如异常信息在日志中是一条日志对应多行内容，此时我们需要用到 iLogtail 的 processor_split_log_regex 多行采集插件进行采集。Spring Boot 已经集成了 Logback 日志框架，默认会读取 logback-spring.xml 配置文件，如果想自定义配置文件的名称，可以在 Spring Boot 配置文件中配置 logging.config 参数指定 Logback 配置文件。
以下为 Logback 配置日志输出和 iLogtail 配置日志采集并写入 Kafka 集群的示例：

## 项目配置

```plain
logging.config = /home/tomcat/conf/logback-spring.xml
```

## Logback 配置

```xml
<?xml version="1.0" encoding="UTF-8"?>
<configuration debug="false" scan="false">
    <!--定义日志路径-->
    <property scope="context" name="LOG_HOME" value="/home/tomcat/logs"/>
    <property scope="context" name="APP_LOG_HOME" value="${LOG_HOME}/app"/>
    <property scope="context" name="ERR_LOG_HOME" value="${LOG_HOME}/err"/>

    <!--定义日志字段格式-->
    <property scope="context" name="APP_PATTERN"
              value='%d{yyyy-MM-dd HH:mm:ss.SSS}|%X{uuid}|%level|%M|%C\:%L|%thread|%replace(%.-2000msg){"(\r|\n)","\t"}|"%.-2000ex{full}"%n'/>
    <property scope="context" name="ERR_PATTERN"
              value='%d{yyyy-MM-dd HH:mm:ss.SSS}|%X{uuid}|%level|%M|%C\:%L|%thread|%replace(%msg){"(\r|\n)","\t"}|"%.-2000ex{full}"%n'/>

    <!--日志保留时间、日志大小、日志轮转等配置-->
    <appender name="APP_FILE" class="ch.qos.logback.core.rolling.RollingFileAppender">
        <file>${APP_LOG_HOME}/app.${HOSTNAME}.ing</file>
        <rollingPolicy class="ch.qos.logback.core.rolling.TimeBasedRollingPolicy">
            <fileNamePattern>${APP_LOG_HOME}/${APP_LOG_TABLE_NAME}.${HOSTNAME}.%d{yyyy-MM-dd_HH}.log.%i
            </fileNamePattern>
            <maxHistory>168</maxHistory>
            <timeBasedFileNamingAndTriggeringPolicy class="ch.qos.logback.core.rolling.SizeAndTimeBasedFNATP">
                <maxFileSize>300MB</maxFileSize>
            </timeBasedFileNamingAndTriggeringPolicy>
            <totalSizeCap>2GB</totalSizeCap>
            <cleanHistoryOnStart>true</cleanHistoryOnStart>
        </rollingPolicy>
        <encoder class="ch.qos.logback.classic.encoder.PatternLayoutEncoder">
            <charset>${CHARSET}</charset>
            <pattern>${APP_PATTERN}</pattern>
        </encoder>
        <!-- immediateFlush 设置为 false 缓冲区满后才落盘，可提高日志写入吞吐量，当日志量非常少时，需要设置为 true, 否则可能会导致日志轮转后丢失日志的情况-->
        <immediateFlush>true</immediateFlush>
    </appender>

    <appender name="ERR_FILE" class="ch.qos.logback.core.rolling.RollingFileAppender">
        <filter class="ch.qos.logback.classic.filter.ThresholdFilter">
            <level>WARN</level>
        </filter>
        <file>${ERR_LOG_HOME}/err.${HOSTNAME}.ing</file>
        <rollingPolicy class="ch.qos.logback.core.rolling.TimeBasedRollingPolicy">
            <fileNamePattern>${ERR_LOG_HOME}/err.${HOSTNAME}.%d{yyyy-MM-dd}.log.%i</fileNamePattern>
            <maxHistory>7</maxHistory>
            <timeBasedFileNamingAndTriggeringPolicy class="ch.qos.logback.core.rolling.SizeAndTimeBasedFNATP">
                <maxFileSize>300MB</maxFileSize>
            </timeBasedFileNamingAndTriggeringPolicy>
            <totalSizeCap>2GB</totalSizeCap>
            <cleanHistoryOnStart>true</cleanHistoryOnStart>
        </rollingPolicy>
        <encoder class="ch.qos.logback.classic.encoder.PatternLayoutEncoder">
            <charset>${CHARSET}</charset>
            <pattern>${ERR_PATTERN}</pattern>
        </encoder>
        <immediateFlush>true</immediateFlush>
    </appender>

    <!--开启异步-->
    <appender name="ASYNC_APP_FILE" class="ch.qos.logback.classic.AsyncAppender">
        <queueSize>1024</queueSize>
        <!-- 日志队列满不阻塞业务线程 -->
        <neverBlock>true</neverBlock>
        <discardingThreshold>512</discardingThreshold>
        <!-- 日志记录类和行号 -->
        <includeCallerData>true</includeCallerData>
        <appender-ref ref="APP_FILE"/>
    </appender>

    <appender name="ASYNC_ERR_FILE" class="ch.qos.logback.classic.AsyncAppender">
          <queueSize>1024</queueSize>
          <discardingThreshold>512</discardingThreshold>
          <includeCallerData>true</includeCallerData>
          <appender-ref ref="ERR_FILE"/>
      </appender>

    <root level="WARN">
        <appender-ref ref="ASYNC_ERR_FILE"/>
    </root>
    <logger name="com.xxx.yyy" level="INFO">
        <appender-ref ref="ASYNC_APP_FILE"/>
    </logger>
</configuration>    

```


## 日志输出样例
### App 日志

```log
2023-09-04 11:00:00.885||INFO|putMethod|com.xxx.yyy.ClassName:99|JobThread-893-1693796400023|写本地日志内容|""
2023-09-04 11:13:50.565|de8139bf-b013-499f-acde-245f9798e73e|WARN|preHandMethod|com.xxx.yyy.ClassName:108|HTTP-exec-21|校验登陆失败，跳转到 login|""
```

### Err 日志

```log
2023-09-04 00:00:08.212|2148dd19-c0ff-4ef3-b32e-cd59602ec44e|ERROR|internalMethod|com.alibaba.druid.filter.stat.StatFilter:523|JobThread-893-1693756800039|slow sql 5676 millis. SELECT SUM(VERIFY_NUM) as totalNum, SUM(VERIFY_MONEY) as totalMoney FROM tb_pay_table[]|""
2023-09-04 00:00:43.079|ed0be4c8-974d-4bf5-9154-1b0ac4edf0b5|ERROR|postMethod|com.xxl.job.core.util.XxlJobRemotingUtil:143|xxl-job, executor ExecutorRegistryThread|Read timed out|"java.net.SocketTimeoutException: Read timed out
	at java.net.SocketInputStream.socketRead0(Native Method)
	at java.net.SocketInputStream.socketRead(SocketInputStream.java:116)
	at java.net.SocketInputStream.read(SocketInputStream.java:171)
	at java.net.SocketInputStream.read(SocketInputStream.java:141)
	at java.io.BufferedInputStream.fill(BufferedInputStream.java:246)
	at java.io.BufferedInputStream.read1(BufferedInputStream.java:286)
	at java.io.BufferedInputStream.read(BufferedInputStream.java:345)
	at sun.net.www.http.HttpClient.parseHTTPHeader(HttpClient.java:735)
	at sun.net.www.http.HttpClient.parseHTTP(HttpClient.java:678)
	at sun.net.www.protocol.http.HttpURLConnection.getInputStream0(HttpURLConnection.java:1593)
	at sun.net.www.protocol.http.HttpURLConnection.getInputStream(HttpURLConnection.java:1498)
	at java.net.HttpURLConnection.getResponseCode(HttpURLConnection.java:480)
	at com.xxl.job.core.util.XxlJobRemotingUtil.postBody(XxlJobRemotingUtil.java:119)
	at com.xxl.job.core.biz.client.AdminBizClient.registry(AdminBizClient.java:42)
	at com.xxl.job.core.thread.ExecutorRegistryThread$1.run(ExecutorRegistryThread.java:48)
	at java.lang.Thread.run(Thread.java:748)
"
```

## 采集配置

### App 日志采集
```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /home/tomcat/logs/app
    FilePattern: app.*.ing
processors:
  - Type: processor_split_log_regex
    SplitRegex: \d+-\d+-\d+\s\d+:\d+:\d+\.\d+\|.*
    SplitKey: content
    PreserveOthers: true
  - Type: processor_split_char
    SourceKey: content
    SplitSep: "|"
    SplitKeys:
      - timestamp
      - uuid
      - level
      - method
      - class_line
      - thread
      - message
      - exception
  - Type: processor_split_char
    SourceKey: class_line
    SplitSep: ":"
    SplitKeys:
      - class
      - line
  - Type: processor_add_fields
    Fields:
      service: logtail-example-app
    IgnoreIfExist: false
flushers:
  - Type: flusher_kafka_v2
    Brokers:
        - 192.168.0.101:9092
        - 192.168.0.102:9092
        - 192.168.0.103:9092
    Topic: APP_LOG
```


### Err 日志采集
```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /home/tomcat/logs/err
    FilePattern: err.*.ing
processors:
  - Type: processor_split_log_regex
    SplitRegex: \d+-\d+-\d+\s\d+:\d+:\d+\.\d+\|.*
    SplitKey: content
    PreserveOthers: true
  - Type: processor_split_char
    SourceKey: content
    SplitSep: "|"
    SplitKeys:
      - timestamp
      - uuid
      - level
      - method
      - class_line
      - thread
      - message
      - exception
  - Type: processor_split_char
    SourceKey: class_line
    SplitSep: ":"
    SplitKeys:
      - class
      - line
  - Type: processor_add_fields
    Fields:
      service: logtail-example-app
    IgnoreIfExist: false
flushers:
  - Type: flusher_kafka_v2
    Brokers:
        - 192.168.0.101:9092
        - 192.168.0.102:9092
        - 192.168.0.103:9092
    Topic: ERR_LOG
```
