# 一次性文件采集

## 简介

`input_static_file_onetime`插件是LoongCollector的OneTime类型输入插件，专门用于一次性文件采集任务。与常规的`input_file`插件不同，该插件执行一次后即完成采集任务，不会持续监控文件变化，适用于历史文件采集、数据迁移、批量数据处理等场景。

## 版本

[Beta](../../stability-level.md)

## 版本说明

* 推荐版本：暂无（可使用edge版体验）

## 核心特性

### OneTime执行模式
- **一次性执行**：插件启动后扫描指定路径下的所有匹配文件，完成采集后自动停止
- **批量处理**：支持同时处理大量文件，无需持续监控
- **资源优化**：避免长期占用系统资源，适合批处理场景

### 强大的文件发现能力
- **通配符支持**：支持`*`和`**`通配符进行文件路径匹配
- **递归搜索**：支持深度目录搜索，可配置最大搜索深度
- **黑名单过滤**：支持文件路径、文件名、目录黑名单过滤


## 配置参数

### 全局配置参数（必填）

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  ExcutionTimeout  |  uint  |  是  |  /  |  OneTime配置执行超时时间（秒）。取值范围：600～604800（10分钟～1周）。超过此时间配置将自动过期并被删除。  |

### 插件配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为input\_static\_file\_onetime。  |
|  FilePaths  |  []string  |  是  |  /  |  待采集的文件路径列表（目前仅限1个路径）。支持使用\*和\*\*通配符。  |
|  MaxDirSearchDepth  |  int  |  否  |  0  |  文件路径中\*\*通配符匹配的最大目录深度。取值范围为0～1000。  |
|  ExcludeFilePaths  |  []string  |  否  |  空  |  文件路径黑名单。支持使用\*通配符。  |
|  ExcludeFiles  |  []string  |  否  |  空  |  文件名黑名单。支持使用\*通配符。  |
|  ExcludeDirs  |  []string  |  否  |  空  |  目录黑名单。支持使用\*通配符。  |
|  FileEncoding  |  string  |  否  |  utf8  |  文件编码格式。可选值包括utf8和gbk。  |
|  Multiline  |  object  |  否  |  空  |  多行聚合选项。详见表1。  |
|  AppendingLogPositionMeta  |  bool  |  否  |  false  |  是否在日志中添加该条日志所属文件的元信息，包括\_\_tag\_\_:\_\_inode\_\_字段和\_\_file\_offset\_\_字段。  |
|  Tags  |  map[string]string  |  否  |  空  |  重命名或删除tag。map中的key为原tag名，value为新tag名。若value为空，则删除原tag。  |

* 表1：多行聚合选项

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Mode  |  string  |  否  |  custom  |  多行聚合模式。可选值包括custom和JSON。  |
|  StartPattern  |  string  |  当Multiline.Mode取值为custom时，至少1个必填  |  空  |  行首正则表达式。  |
|  ContinuePattern  |  string  |  |  空  |  行继续正则表达式。  |
|  EndPattern  |  string  |  |  空  |  行尾正则表达式。  |
|  UnmatchedContentTreatment  |  string  |  否  |  single_line  |  对于无法匹配的日志段的处理方式，可选值如下：<ul><li>discard：丢弃</li><li>single_line：将不匹配日志段的每一行各自存放在一个单独的事件中</li></ul>   |



## 使用场景

### 历史文件采集
- **日志归档**：将历史日志文件批量采集到新的存储系统
- **数据备份**：对重要历史数据进行备份采集
- **合规审计**：满足数据保留和审计要求

### 数据迁移与处理
- **数据迁移**：将数据从旧系统迁移到新系统
- **数据分析**：对数据进行批量分析处理
- **数据清洗**：对数据进行格式化和清洗
- **数据转换**：将数据转换为新的格式

### 临时采集任务
- **一次性采集**：执行临时的一次性数据采集任务
- **测试验证**：用于测试和验证采集配置


## 样例

### 基础历史文件采集

采集 `/var/log/history `目录下所有 `.log `文件，并将结果输出至stdout。

**配置文件位置**：`/opt/loongcollector/conf/onetime_pipeline_config/local/historical_logs.yaml`

```yaml
enable: true
global:
  ExcutionTimeout: 3600  # 1小时超时
inputs:
  - Type: input_static_file_onetime
    FilePaths: 
      - /var/log/history/*.log
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

### 递归目录搜索

采集 `/var/log/archive `目录下所有子目录中的 `.log `文件，最大搜索深度为5层。

```yaml
enable: true
global:
  ExcutionTimeout: 7200  # 2小时超时
inputs:
  - Type: input_static_file_onetime
    FilePaths: 
      - /var/log/archive/**/*.log
    MaxDirSearchDepth: 5
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```



### 多行日志处理

采集包含多行日志的历史文件，使用正则表达式进行日志聚合。

```yaml
enable: true
global:
  ExcutionTimeout: 1800  # 30分钟超时
inputs:
  - Type: input_static_file_onetime
    FilePaths: 
      - /var/log/history/*.log
    Multiline:
      StartPattern: ^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}
      UnmatchedContentTreatment: single_line
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

### 文件过滤配置

采集时排除特定文件和目录。

```yaml
enable: true
global:
  ExcutionTimeout: 600  # 10分钟超时
inputs:
  - Type: input_static_file_onetime
    FilePaths: 
      - /var/log/**/*.log
    ExcludeFiles:
      - "*.tmp"
      - "*.bak"
    ExcludeDirs:
      - "/var/log/temp"
      - "/var/log/backup"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

## 性能优化建议

### 文件路径优化
- 使用精确的文件路径匹配，避免过于宽泛的通配符
- 合理设置 `MaxDirSearchDepth `，避免过深的目录搜索
- 使用黑名单过滤不需要的文件，减少处理开销

### 内存管理
- 对于大文件，考虑分批处理
- 合理配置多行日志聚合参数，避免内存溢出
- 监控采集过程中的内存使用情况

### 并发控制
- 根据系统资源情况调整并发处理数量
- 避免同时处理过多大文件
- 合理设置超时参数

## OneTime配置限制与注意事项

### 执行时机与超时机制
1. **执行时机**：OneTime配置在LoongCollector启动时立即执行，不会等待其他条件
2. **超时时间**：必须在global配置中设置`ExcutionTimeout`参数，取值范围：600～604800秒（10分钟～1周）
3. **自动过期**：超过超时时间后，配置将自动过期并被删除，不会重复执行
4. **状态管理**：系统会记录配置的执行状态，避免重复执行相同的配置

### 配置要求
1. **全局配置**：必须在global配置中设置`ExcutionTimeout`参数，否则配置不会生效
2. **配置文件位置**：OneTime配置文件必须放在指定目录下才能被LoongCollector识别
3. **文件完整性**：确保在采集过程中文件不会被修改或删除
4. **权限要求**：确保LoongCollector有足够的文件读取权限
5. **资源消耗**：大量文件采集时注意系统资源使用情况

### 配置文件存放目录
OneTime配置文件必须放在以下目录中：

**默认配置目录**：
- 路径：`{LoongCollector安装目录}/conf/onetime_pipeline_config/local/`
- 例如：`/opt/loongcollector/conf/onetime_pipeline_config/local/`

**自定义配置目录**：
- 可通过环境变量`LOONG_CONF_DIR`自定义配置目录
- 实际路径：`{LOONG_CONF_DIR}/onetime_pipeline_config/local/`

**配置文件命名**：
- 支持`.yaml`和`.json`格式
- 建议使用有意义的文件名，如`historical_log_collection.yaml`

### 使用限制
1. **一次性执行**：每个OneTime配置只能执行一次，执行完成后自动停止
2. **配置隔离**：OneTime配置与常规持续配置完全隔离，不会相互影响
3. **文件发现**：启动时一次性发现所有匹配文件，不会动态监控新文件
4. **错误处理**：执行过程中出现错误不会自动重试，需要重新部署配置

## 与input_file的区别

| 特性 | input_static_file_onetime | input_file |
| --- | --- | --- |
| 执行模式 | OneTime（一次性） | Continuous（持续） |
| 适用场景 | 历史文件批量采集 | 实时日志监控 |
| 资源占用 | 临时占用，采集完成后释放 | 持续占用，长期监控 |
| 文件发现 | 启动时一次性发现所有文件 | 持续监控文件变化 |
| 性能特点 | 批量处理，适合大量文件 | 实时处理，适合持续监控 |

通过合理使用`input_static_file_onetime`插件，可以高效地完成一次性文件采集任务，为历史文件采集、数据迁移、备份和分析提供强有力的支持。
