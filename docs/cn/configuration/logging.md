# 日志

**LoongCollector** 由 C++ 主进程与 Go 扩展插件等组成，运行日志分为 **C++ 主进程日志**（spdlog，配置为 `apsara_log_conf.json`）与 **插件 / Go 侧日志**（seelog，配置为 `plugin_logger.xml`）。

**路径约定**：下文 `{conf}` 表示配置根目录（为 `$LOONGCOLLECTOR_ROOT/conf`），`{log}` 表示日志根目录（标准安装布局为 `$LOONGCOLLECTOR_ROOT/log`）。完整目录树见 [目录结构说明](../installation/loongcollector-dir.md)。  
开启 **`logtail_mode`** 时，部分路径与日志名前缀会回退为历史 iLogtail 布局（如配置目录即为进程工作目录、主日志可能为 `ilogtail.LOG`、Logger 前缀为 `/apsara/sls/ilogtail`），下面默认按 **非 logtail_mode 的 LoongCollector 布局**描述。

---

## 主进程日志（C++ / spdlog）

| 项 | 说明 |
| --- | --- |
| 配置文件 | `{conf}/apsara_log_conf.json` |
| 启动诊断 | 在加载 `apsara_log_conf.json` **之前**，会向 `{log}/logger_initialization.log` 写入初始化信息。 |
| 主日志文件 | **`{log}/loongcollector.LOG`** |
| Logger 名前缀 | **`/apsara/loongcollector`** |
| 根 Logger | **`/`**，默认用作兜底 Logger。 |

**级别**（写入 JSON 配置及环境变量映射）：`TRACE`、`DEBUG`、`INFO`、`WARNING`、`ERROR`、`FATAL`。

**环境变量 `LOGTAIL_LOG_LEVEL`**：若值为上述之一，成功解析后对 **主采集 Logger** 覆盖 JSON 中的级别。

**默认行为**：若 `apsara_log_conf.json` 缺失或解析失败，会生成 spdlog **默认配置**并尽量写回该文件：`/` 使用 `WARNING`，**`/apsara/loongcollector`** 使用 `INFO`，共用 `AsyncFileSink`，落盘路径为 **`{log}/loongcollector.LOG`**。

**参考配置结构**（以落盘为准）：

```json
{
	"Loggers": {
		"/": {
			"AsyncFileSink": "WARNING"
		},
		"/apsara/loongcollector": {
			"AsyncFileSink": "INFO"
		}
	},
	"Sinks": {
		"AsyncFileSink": {
			"Type": "AsyncFile",
			"MaxLogFileNum": 10,
			"MaxLogFileSize": 20000000,
			"MaxDaysFromModify": 300,
			"LogFilePath": "/opt/loongcollector/log/loongcollector.LOG",
			"Compress": "Gzip"
		}
	}
}
```

---

## 插件日志（Go / seelog）

| 项 | 说明 |
| --- | --- |
| 配置文件 | **`{conf}/plugin_logger.xml`** |
| 默认日志文件 | **`{log}/go_plugin.LOG`** |
| 若 XML 不存在 | 会按模板 **自动生成** `plugin_logger.xml`。 |

**级别**：seelog 使用小写等级名（如 **`trace`、`debug`、`info`、`warn`、`error`、`critical`**）。默认生产选项为异步 Logger + **`info`** 级别。

**环境变量 `LOGTAIL_LOG_LEVEL`**：在加载 XML 后，会把配置中的 **`minlevel="..."`** 替换为该环境变量的值（小写），用于临时调试；需为 seelog 可识别的等级字符串。

**参考配置**：

```xml
<seelog type="asynctimer" asyncinterval="500000" minlevel="info" >
 <outputs formatid="common">
  <rollingfile type="size" filename="/opt/loongcollector/log/go_plugin.LOG" maxsize="20000000" maxrolls="10"/>
 </outputs>
 <formats>
  <format id="common" format="%Date %Time [%LEV] [%File:%Line] [%FuncShort] %Msg%n" />
 </formats>
</seelog>
```

更多与插件开发相关的 Logger 用法见 [打印日志](../developer-guide/plugin-development/plugin-debug/logger-api.md)（其中命令行与文件若仍出现 `ilogtail` 字样的占位，指历史路径表述，实际文件以本文与部署目录为准）。
