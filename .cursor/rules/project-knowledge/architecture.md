# 项目结构

## Architecture

The LoongCollector architecture is based on a plugin system with the following key components:

1. **Core Application**: The main entry point is in `core/logtail.cpp` which initializes the `Application` class in `core/application/Application.cpp`. The application follows a singleton pattern and manages the overall lifecycle.

2. **Plugin System**: The system supports plugins for data collection, processing, and flushing:
   - **Inputs**: Collect data from various sources (files, network, system metrics, etc.)
   - **Processors**: Transform and process collected data
   - **Flushers**: Send processed data to various backends

3. **Pipeline Management**: The system uses collection pipelines managed by `CollectionPipelineManager` to handle data flow from inputs through processors to flushers.

4. **Configuration**: Supports both local and remote configuration management with watchers that monitor for configuration changes.

5. **Queuing System**: Implements various queue types including bounded queues, circular queues, and exactly-once delivery queues for reliable data transmission.

6. **Monitoring**: Built-in monitoring and metrics collection for tracking the collector's own performance and health.

## 项目结构

- core: 项目核心代码目录，包含所有主流程、插件、配置、监控、采集、处理等模块
  - plugin: 插件体系，包含输入、处理、输出（flusher）等插件实现
    - input: 数据采集输入插件
    - processor: 数据处理插件
    - flusher: 数据输出插件（如 SLS、文件等）
      - sls: 阿里云 SLS 输出插件
        - FlusherSLS.cpp/h: SLS 输出插件主实现
        - SLSClientManager.cpp/h: SLS 客户端管理器基类
        - EnterpriseSLSClientManager.cpp/h: 企业版 SLS 客户端管理器
        - SLSConstant.cpp/h: SLS 相关常量定义
        - SLSUtil.cpp/h: SLS 工具函数
        - SLSResponse.cpp/h: SLS 响应处理
        - SendResult.cpp/h: 发送结果处理
        - Exception.h: SLS 异常定义
        - PackIdManager.cpp/h: 包 ID 管理器
        - DiskBufferWriter.cpp/h: 磁盘缓冲区写入器
        - EnterpriseFlusherSLSMonitor.cpp/h: 企业版 SLS 监控
        - EnterpriseInternalMetricAggregator.cpp/h: 企业版内部指标聚合器
  - collection_pipeline: 采集主流程管道，包含队列、批处理、序列化等
  - config: 配置相关，包含配置加载、Provider、反馈等
    - provider: 配置提供者实现
      - ConfigProvider.cpp/h: 配置提供者基类
      - EnterpriseConfigProvider.cpp/h: 企业版配置提供者，支持远程配置获取、端点探测、配置更新等
      - LegacyConfigProvider.cpp/h: 传统配置提供者，支持本地配置文件监控和配置转换
      - ConfigConversionUtil.cpp/h: 配置转换工具，负责新旧配置格式之间的转换
  - common: 通用工具、数据结构、网络、字符串、加解密等
  - monitor: 监控与指标采集、报警
  - logger: 日志系统
  - checkpoint: 断点续传、状态管理
  - app_config: 全局配置管理
  - models: 事件、日志、指标等核心数据结构
  - parser: 日志解析器
  - task_pipeline: 任务调度与管理
  - go_pipeline: Go 插件适配与集成
  - ebpf: eBPF 相关采集与插件
  - host_monitor: 主机级监控采集
  - shennong: 神农指标采集与转换
  - prometheus: Prometheus 采集与适配
  - file_server: 文件采集与管理
  - container_manager: 容器环境相关管理
  - daemon: 守护进程与服务管理
  - application: 应用主入口
  - protobuf: Protobuf 协议定义与生成代码
  - metadata: K8s 等元数据采集
  - constants: 常量定义
  - tools: 内部工具脚本
  - unittest: 单元测试代码
  - legacy_test: 历史测试用例
- bin: 可执行文件输出目录
- docs: 项目文档
- example_config: 示例配置文件
- scripts: 构建、部署、测试等脚本
- docker: Docker 相关文件
- rpm: RPM 打包相关
- external: 外部依赖
- pkg: Go 相关包
  - helper: Go相关帮助函数
    - containercenter: Go容器相关函数实现
- plugin_main: 插件主入口
- pluginmanager: Go 插件管理器，负责插件的生命周期和注册
- plugins: Go 插件包，包含 input、processor、flusher 等插件的具体实现
  - input: 数据采集输入插件
    - docker: Docker 容器数据采集
      - stdout: Docker 标准输出采集
      - event: Docker 事件采集
      - logmeta: Docker 日志元数据
      - rawstdout: Docker 原始标准输出
  - processor: 数据处理插件
  - flusher: 数据输出插件
  - aggregator: 数据聚合插件
  - extension: 扩展功能
  - all: 插件注册与初始化
  - test: 测试相关
- test: 集成测试

## 依赖库

## 依赖库目录结构

- **系统依赖库路径**:
  - `/usr/local/include` - 系统头文件
  - `/usr/local/lib` - 系统库文件
- **自定义依赖库路径**:
  - `/opt/logtail/deps/` - 项目专用依赖根目录
    - `include/` - 头文件目录
    - `lib/` - 静态/动态库文件
    - `bin/` - 可执行文件和工具

## 依赖库分类与管理

### Header-Only 库

仅需包含头文件，无需链接：

- `spdlog` - 日志库
- `rapidjson` - JSON 解析库

### 编译依赖库

需要编译和链接的外部库：

- **测试框架**: `gtest`, `gmock`
- **序列化**: `protobuf`
- **正则表达式**: `re2`
- **哈希算法**: `cityhash`
- **配置解析**: `jsoncpp`, `yamlcpp`
- **压缩算法**: `lz4`, `zlib`, `zstd`
- **网络通信**: `curl`, `ssl`, `crypto`
- **系统工具**: `boost`, `gflags`, `leveldb`, `uuid`
- **内存管理**: `tcmalloc` (可选)

### 平台特定库

- **Linux**: `pthread`, `dl`, `rt`, `uuid`
- **Windows**: `Psapi.lib`, `ws2_32.lib`, `Rpcrt4.lib`, `Shlwapi.lib`

## 主要技术栈

- C++: 主体实现语言
- Protobuf: 数据结构序列化
- eBPF: 内核级数据采集
- Prometheus: 指标采集与监控
- Go: 插件适配与部分实现
- Shell/Python: 构建与测试脚本
