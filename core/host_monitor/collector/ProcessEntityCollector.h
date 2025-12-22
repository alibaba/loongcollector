/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/collector/BaseCollector.h"
#include "monitor/metric_models/MetricRecord.h"

namespace logtail {

// 前向声明
class ProcessEntityConfigManager;

// 进程主键（唯一标识）
struct ProcessPrimaryKey {
    pid_t pid = 0;
    int64_t startTime = 0; // 进程启动时间(ticks)

    bool operator==(const ProcessPrimaryKey& other) const { return pid == other.pid && startTime == other.startTime; }

    bool operator!=(const ProcessPrimaryKey& other) const { return !(*this == other); }
};

// ProcessPrimaryKey的哈希函数
struct ProcessPrimaryKeyHash {
    std::size_t operator()(const ProcessPrimaryKey& key) const {
        return std::hash<pid_t>()(key.pid) ^ (std::hash<int64_t>()(key.startTime) << 1);
    }
};

// 进程实体信息
struct ProcessEntityInfo {
    // ====== 进程主键（唯一标识） ======
    pid_t pid = 0;
    int64_t startTime = 0; // 进程启动时间ticks（不变）

    // ====== 进程标识属性（不变） ======
    uint32_t flags = 0; // Process flags (不变，用于判断内核线程等)
    std::string comm; // 进程名称（不变）
    std::string exe; // 可执行文件路径（不变，/proc/[pid]/exe）
    std::string cmdline; // 完整命令行（不变，/proc/[pid]/cmdline）
    std::vector<std::string> args; // 命令行参数（不变）

    // ====== 可变属性（全量上报时重新读取） ======
    pid_t ppid = 0; // 父进程ID（会变：父进程退出后被收养）
    std::string user; // 用户名（会变：如nginx降权）
    char state = '\0'; // 进程状态（会变：R/S/D/Z/T等）
    bool isZombie = false; // 是否僵尸进程（状态为Z）

    // ====== 元数据 ======
    time_t firstObservedTime = 0; // 首次观察时间（Unix时间戳）
    time_t lastReportTime = 0; // 最后一次上报时间（Unix时间戳）
    time_t lastVariableUpdateTime = 0; // 最后一次更新可变属性的时间
    int64_t startTimeUnix = 0; // 进程启动Unix时间戳（用于计算运行时间）
};

// 进程过滤配置
struct ProcessFilterConfig {
    // 白名单:进程名或可执行文件路径匹配（为空则不过滤）
    std::vector<std::string> whitelistPatterns;

    // 黑名单:进程名或可执行文件路径匹配（为空则不过滤）
    std::vector<std::string> blacklistPatterns;

    // 预编译的正则表达式对象（缓存，避免重复编译）
    std::vector<std::regex> compiledWhitelistRegexes;
    std::vector<std::regex> compiledBlacklistRegexes;

    // 快速路径：简单字符串模式（不包含正则特殊字符）
    std::vector<std::string> simpleWhitelistPatterns;
    std::vector<std::string> simpleBlacklistPatterns;

    // 最小运行时间(秒):过滤掉运行时间过短的进程
    // 0 表示不过滤，默认 20 秒
    int64_t minRunningTimeSeconds = 20;

    // 是否排除内核线程，默认 true
    bool excludeKernelThreads = true;

    // 编译正则表达式（在配置加载时调用一次）
    // 返回 true 表示成功，false 表示有无效的正则表达式
    bool CompileRegexes();

private:
    // 判断模式是否为简单字符串（不包含正则特殊字符）
    static bool IsSimplePattern(const std::string& pattern);
};

class ProcessEntityCollector : public BaseCollector {
public:
    ProcessEntityCollector();
    ~ProcessEntityCollector() override = default;

    bool Init(HostMonitorContext& collectContext) override;
    bool Collect(HostMonitorContext& collectContext, PipelineEventGroup* groupPtr) override;
    [[nodiscard]] const std::string& Name() const override { return sName; }
    [[nodiscard]] const std::chrono::seconds GetCollectInterval() const override;

    static const std::string sName;

    // 静态方法：设置全局配置
    static void RegisterConfig(const std::string& configName,
                               const ProcessFilterConfig& filterConfig,
                               std::chrono::seconds fullReportInterval);
    static void UnregisterConfig(const std::string& configName);
    static void TriggerFullReport(const std::string& configName);

private:
    // 全量采集（定期上报所有缓存的进程实体）
    void FullCollect(HostMonitorContext& collectContext, PipelineEventGroup* groupPtr);

    // 增量采集（只检测新增/退出/PID复用，不检测属性变化）
    void IncrementalCollect(HostMonitorContext& collectContext, PipelineEventGroup* groupPtr);

    // 检测进程变化
    struct ProcessChanges {
        std::vector<ProcessPrimaryKey> added; // 新增进程
        std::vector<ProcessPrimaryKey> removed; // 退出进程
        std::vector<ProcessPrimaryKey> reused; // PID被复用的进程
    };

    ProcessChanges DetectChanges(const std::unordered_map<pid_t, ProcessPrimaryKey>& oldPidMap,
                                 const std::unordered_map<pid_t, ProcessPrimaryKey>& currentPidMap);

    // 获取当前所有进程的主key信息
    std::unordered_map<pid_t, ProcessPrimaryKey> GetCurrentProcessPrimaryKeys(time_t now);

    // 获取进程实体信息（包含不变属性和初始值）
    bool GetProcessEntityInfo(pid_t pid, time_t now, ProcessEntityInfo& info);

    // 更新进程可变属性（ppid、user、state等）
    bool UpdateVariableAttributes(pid_t pid, time_t now, ProcessEntityInfo& info);

    // 进程过滤
    bool ShouldCollectProcess(pid_t pid, const ProcessEntityInfo& info);

    // 生成进程实体Event（包含不变属性和可变属性）
    void GenerateProcessEntityEvent(PipelineEventGroup* group, const ProcessEntityInfo& info, bool isFullReport);

    // 添加主机标签
    void AddHostLabels(PipelineEventGroup& group);

    // 全局配置存储（静态，所有实例共享）
    struct GlobalConfig {
        ProcessFilterConfig filterConfig;
        std::chrono::seconds fullReportInterval{3600};
        bool forceFullReport = false; // 用于测试：强制触发全量上报
    };
    static std::mutex sConfigMutex;
    static std::unordered_map<std::string, GlobalConfig> sConfigs;

    // 实例配置（从全局配置加载）
    std::string mConfigName; // 配置名称（用于查找全局配置）
    ProcessFilterConfig mFilterConfig;
    // 注意：全量上报间隔不再缓存为成员变量，每次从全局配置动态读取以支持实时更新

    // 缓存:存储已采集的进程实体信息（永久保存，只在进程退出时清理）
    std::unordered_map<ProcessPrimaryKey, ProcessEntityInfo, ProcessPrimaryKeyHash> mProcessCache;

    // PID 索引：快速查找 pid -> ProcessPrimaryKey（与 mProcessCache 同步维护）
    std::unordered_map<pid_t, ProcessPrimaryKey> mPidToKeyIndex;

    // 采集状态（使用 mutex 保护以防并发访问）
    mutable std::mutex mStateMutex;
    std::chrono::steady_clock::time_point mLastFullReportTime;
    bool mIsFirstCollect = true;

    // 指标 (静态，所有实例共享同一组指标)
    static std::once_flag sMetricsInitFlag;
    static MetricsRecordRef sMetricsRecordRef;
    static CounterPtr sFullReportTotal;
    static CounterPtr sIncrementalReportTotal;
    static CounterPtr sProcessAddedTotal;
    static CounterPtr sProcessRemovedTotal;
    static CounterPtr sProcessReusedTotal;
    static IntGaugePtr sCachedProcessCount;
    static TimeCounterPtr sCollectLatencyMs;
    static TimeCounterPtr sFullCollectLatencyMs;
    static TimeCounterPtr sIncrementalCollectLatencyMs;

    // 初始化指标（线程安全，只执行一次）
    static void InitMetrics();

    // 允许 ProcessEntityConfigManager 访问静态成员以支持动态配置更新
    friend class ProcessEntityConfigManager;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessEntityCollectorUnittest;
    friend class ProcessEntityConfigManagerUnittest;
#endif
};

} // namespace logtail
