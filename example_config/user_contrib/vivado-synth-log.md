# Vivado 综合日志

## 提供者

[4lex49](https://github.com/4lex49)

## 描述

**Vivado**®设计套件是Xilinx公司开发的一种用于HDL（Hardware Description Language，硬件描述语言）设计的软件套件。在FPGA开发流程中，HDL设计完毕后，通过Vivado提供的相关工具执行**综合**操作，对HDL代码进行进一步分析，可将RTL转化为门级电路表示。

综合过程中，Vivado会生成一系列日志以向硬件开发者输出综合状态，报告警告以及错误等。此日志一般记录在综合目录下的`runme.log`文件中。

* 实际综合时，Vivado输出的日志格式较为杂乱。采用`processor_filter_regex`过滤出综合过程本身需要关注的日志信息，即筛选包含有`Synth`字段的日志信息进行分析

## 日志输入样例

```plain
WARNING: [Synth 8-6014] Unused sequential element lastBatchHistory_1_reg was removed.  [D:/Desktop/Learning/PointCloud/vivado_proj/FPS_Without_IP/FPS_Without_IP.srcs/sources_1/imports/FPS/Wrapper.v:18281]
WARNING: [Synth 8-6014] Unused sequential element lastBatchHistory_2_reg was removed.  [D:/Desktop/Learning/PointCloud/vivado_proj/FPS_Without_IP/FPS_Without_IP.srcs/sources_1/imports/FPS/Wrapper.v:18282]
INFO: [Synth 8-6155] done synthesizing module 'FunctionArray' (20#1) [D:/Desktop/Learning/PointCloud/vivado_proj/FPS_Without_IP/FPS_Without_IP.srcs/sources_1/imports/FPS/Wrapper.v:17445]
INFO: [Synth 8-6157] synthesizing module 'BucketSpliter' [D:/Desktop/Learning/PointCloud/vivado_proj/FPS_Without_IP/FPS_Without_IP.srcs/sources_1/imports/FPS/Wrapper.v:12362]
WARNING: [Synth 8-6779] Delay data for wire-load model not found, using default value
```

## 日志输出样例

```json
{
    "__tag__:__path__":"/mnt/hgfs/synth_25/runme.log","log_type":"WARNING",
    "log_id":"Synth 8-6014",
    "content":"Unused sequential element lastBatchHistory_1_reg was removed. ",
     "path":"D:/Desktop/Learning/PointCloud/vivado_proj/FPS_Without_IP/FPS_Without_IP.srcs/sources_1/imports/FPS/Wrapper.v:18281",
    "__time__":"1689234234"
}
{
    "__tag__:__path__":"/mnt/hgfs/synth_25/runme.log",
    "log_type":"WARNING",
    "log_id":"Synth 8-6014",
    "content":"Unused sequential element lastBatchHistory_2_reg was removed. ",
    "path":"D:/Desktop/Learning/PointCloud/vivado_proj/FPS_Without_IP/FPS_Without_IP.srcs/sources_1/imports/FPS/Wrapper.v:18282",
    "__time__":"1689234234"
}
{
    "__tag__:__path__":"/mnt/hgfs/synth_25/runme.log",
    "log_type":"INFO",
    "log_id":"Synth 8-6155",
    "content":"done synthesizing module 'FunctionArray' (20#1)",
    "path":"D:/Desktop/Learning/PointCloud/vivado_proj/FPS_Without_IP/FPS_Without_IP.srcs/sources_1/imports/FPS/Wrapper.v:17445",
    "__time__":"1689234234"
}
{
    "__tag__:__path__":"/mnt/hgfs/synth_25/runme.log",
    "log_type":"INFO",
    "log_id":"Synth 8-6157",
    "content":"synthesizing module 'BucketSpliter'",
    "path":"D:/Desktop/Learning/PointCloud/vivado_proj/FPS_Without_IP/FPS_Without_IP.srcs/sources_1/imports/FPS/Wrapper.v:12362",
    "__time__":"1689234234"
}
{
    "__tag__:__path__":"/mnt/hgfs/synth_25/runme.log",
    "log_type":"WARNING",
    "log_id":"Synth 8-6779",
    "content":"Delay data for wire-load model not found, using default value\r",
    "__time__":"1689234242"
}
```

## 采集配置

```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: "/mnt/hgfs/synth_25"
    FilePattern: runme.log
processors:
  - Type: processor_filter_regex
    Include:
      content: '^.+Synth.+$'
  - Type: processor_regex
    SourceKey: content
    Regex: '(\S+): \[(\S+ \S+)\] (?:(.+) \[(\S+)\]|(.+))'
    Keys:
      - log_type
      - log_id
      - content
      - path
      - content
flushers:
  - Type: flusher_stdout
    FileName: ./output.log
```

* 注：该样例是部署于VMWare的Ubuntu系统中的ilogtail主程序对主机（Windows）中的某Vivado工程的综合日志采样得到。其中，主机中的综合目录通过VMWare提供的网络共享功能被分享至Ubuntu的`/mnt/hgfs`目录下
