# LoongCollector 的 目录结构说明

## 产品更名说明

作为 2024 年产品规划的重要组成部分，iLogtail 产品将正式更名为 LoongCollector。此次更新主要包含以下变更：

- 程序二进制文件由 iLogtail 更名为 LoongCollector

- 全面优化目录结构和配置文件布局，提供更清晰的组织方式

## 新版目录结构

LoongCollector 采用模块化的分层目录设计，以下展示了安装在 /opt/loongcollector 下的标准目录结构：

库文件：

- `/opt/loongcollector/libPluginAdapter.so`

- `/opt/loongcollector/libPluginBase.so`

自带证书：`/opt/loongcollector/ca-bundle.crt`

**配置文件目录：**`/opt/loongcollector/conf`

日志配置文件：

- `/opt/loongcollector/conf/apsara_log_conf.json`

- `/opt/loongcollector/conf/plugin_logger.xml`

标识配置文件：

- `/opt/loongcollector/conf/user_defined_id`

采集配置文件：`/opt/loongcollector/conf/continuous_pipeline_config`

进程级文件：`/opt/loongcollector/conf/instance_config`

**数据目录：**`/opt/loongcollector/data`

检查点：

- `/opt/loongcollector/data/go_plugin_checkpoint`

- `/opt/loongcollector/data/exactly_once_checkpoint`

- `/opt/loongcollector/data/file_check_point`

容器路径映射：`/opt/loongcollector/data/docker_path_config.json`

未发送数据：`/opt/loongcollector/data/send_buffer_file_xxxxxxxxxxxx`

Crash临时文件：`/opt/loongcollector/data/backtrace.dat`

**日志目录：**`/opt/loongcollector/log`

主要日志：`/opt/loongcollector/log/loongcollector.log`

Go插件日志：`/opt/loongcollector/log/go_plugin.log`

日志库初始化日志：`/opt/loongcollector/log/logger_initialization.log`

Profile日志：`/opt/loongcollector/log/snapshot`

**run目录：**`/opt/loongcollector/run`

Pid文件：`/opt/loongcollector/run/loongcollector.pid`

inotify日志：`/opt/loongcollector/run/inotify_watcher_dirs`

进程信息日志：`/opt/loongcollector/run/app_info.json`

```plaintext
/
└── /opt/loongcollector/
                       ├── loongcollector                 # 主程序
                       ├── libPluginAdapter.so
                       ├── libPluginBase.so
                       ├── ca-bundle.crt
                       ├── plugins/                       # 插件目录
                       │      └── custom plugins          # 自定义插件
                       ├── dump                           # 仅由 service_http_server 输入插件使用
                       ├── thirdparty/                    # 第三方依赖
                       │      ├── jvm
                       │      └── telegraf
                       ├── conf/                          # 配置目录
                       │      ├── scripts
                       │      ├── apsara_log_conf.json
                       │      ├── plugin_logger.xml
                       │      ├── user_defined_id
                       │      ├── authorization.json
                       │      ├── continuous_pipeline_config/
                       │      │                 ├── local/
                       │      │                 │         └── collect_stdout.json
                       │      │                 └── remote/
                       │      │                           └── collect_file.json
                       │      └── instance_config/
                       │                        ├── local/
                       │                        │         ├── loongcollector_config.json（loongcollector配置）
                       │                        │         └── ebpf.json
                       │                        └── remote/
                       │                                  ├── region.json
                       │                                  └── resource.json
                       ├── data/                                    # 数据目录
                       │       ├── file_check_point                 # 文件采集的checkpoint
                       │       ├── exactly_once_checkpoint/
                       │       ├── go_plugin_checkpoint/            # go插件采集的checkpoint
                       │       ├── docker_path_config.json
                       │       ├── send_buffer_file_xxxxxxxxxxxx
                       │       └── backtrace.dat
                       ├── log/                                     # 日志目录
                       │       ├── loongcollector.log
                       │       ├── loongcollector.log.1
                       │       ├── go_plugin.log
                       │       ├── go_plugin.log.1
                       │       ├── logger_initialization.log
                       │       └── snapshot/
                       └── run/
                               ├── loongcollector.pid
                               ├── inotify_watcher_dirs
                               └── app_info.json
```

## 目录自定义配置

### 支持的自定义目录参数

LoongCollector 提供以下参数用于自定义各类目录位置：

- `loongcollector_conf_dir`: 配置目录

- `loongcollector_log_dir`: 日志目录

- `loongcollector_data_dir`: 数据目录

- `loongcollector_run_dir`: 运行时目录

- `loongcollector_third_party_dir`: 第三方依赖目录

### 配置方式

1. 命令行参数:

```bash
./loongcollector --loongcollector_conf_dir=/custom/path/conf
```

2. 环境变量:

```bash
export loongcollector_conf_dir=/custom/path/conf
./loongcollector
```

## 命名变更对照表

为确保命名一致性，我们对以下文件和目录进行了规范化命名：

| 文件/目录作用            | 原命名                  | 新命名                      |
| ------------------------ | ----------------------- | --------------------------- |
| agent可观测文件          | logtail_monitor_info    | loongcollector_monitor_info |
| go插件采集的checkpoint   | checkpoint              | go_plugin_checkpoint        |
| go插件运行日志           | logtail_plugin.LOG      | go_plugin.LOG               |
| 采集配置目录名           | config                  | continuous_pipeline_config  |
| exactly_once的checkpoint | checkpoint_v2           | exactly_once_checkpoint     |
| agent的发送缓冲buffer文件    | logtail_buffer_file_xxx | send_buffer_file_xxx        |
| agent可观测文件          | ilogtail_status.LOG     | loongcollector_status.LOG   |
| agent运行日志            | ilogtail.LOG            | loongcollector.LOG          |

## 配置兼容性说明

为简化配置体系，以下原 Logtail 配置项将不再默认支持：

- sls_observer_ebpf_host_path

- logtail_snapshot_dir

- inotify_watcher_dirs_dump_filename

- local_event_data_file_name

- crash_stack_file_name

- check_point_filename

- adhoc_check_point_file_dir

- app_info_file

- ilogtail_config

- ilogtail_config_env_name

- logtail_sys_conf_dir

- ALIYUN_LOGTAIL_SYS_CONF_DIR

- ilogtail_docker_file_path_config

## 升级建议

1. **兼容模式**: 如需保持与 Logtail 的兼容性,请参考 [LoongCollector 的 Logtail 兼容模式使用指南](logtail-mode.md)

2. **新版迁移**: 如果选择使用新版目录结构:
   - 建议先备份原有配置和数据

   - 按新版目录结构迁移文件

   - 更新相关配置引用

   - 验证服务正常运行

为确保平稳迁移，我们建议您：

1. 先在测试环境进行充分验证

2. 选择业务低峰期进行升级

3. 做好配置和数据的备份

4. 逐步迁移，避免一次性升级所有实例

5. 密切监控日志采集状态

> **注意**: 迁移过程中请确保数据完整性,建议先在测试环境中进行测试，并非高峰期进行升级操作。