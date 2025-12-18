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
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "collection_pipeline/queue/QueueKey.h"
#include "common/ThreadPool.h"
#include "host_monitor/HostMonitorContext.h"
#include "monitor/metric_models/MetricRecord.h"
#include "runner/InputRunner.h"

namespace logtail {

// Forward declaration
struct ProcessEntityCollectContext;
using ProcessEntityCollectContextPtr = std::shared_ptr<ProcessEntityCollectContext>;

struct ProcessPrimaryKey {
    pid_t pid = 0;
    int64_t startTime = 0; // 进程启动时间(ticks)

    bool operator==(const ProcessPrimaryKey& other) const { return pid == other.pid && startTime == other.startTime; }

    bool operator!=(const ProcessPrimaryKey& other) const { return !(*this == other); }
};

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

    // 注意：ppid、user、state 虽然是可变属性，但增量采集时不追踪其变化
    //       只在全量采集时重新读取并更新

    // ====== 元数据 ======
    time_t firstObservedTime = 0; // 首次观察时间（Unix时间戳）
    time_t lastReportTime = 0; // 最后一次上报时间（Unix时间戳）
    time_t lastVariableUpdateTime = 0; // 最后一次更新可变属性的时间
    int64_t startTimeUnix = 0; // 进程启动Unix时间戳（用于计算运行时间）
};

// ProcessPrimaryKey的哈希函数
struct ProcessPrimaryKeyHash {
    std::size_t operator()(const ProcessPrimaryKey& key) const {
        return std::hash<pid_t>()(key.pid) ^ (std::hash<int64_t>()(key.startTime) << 1);
    }
};

// 进程过滤配置
struct ProcessFilterConfig {
    // 白名单:进程名或可执行文件路径匹配（为空则不过滤）
    std::vector<std::string> whitelistPatterns;

    // 黑名单:进程名或可执行文件路径匹配（为空则不过滤）
    std::vector<std::string> blacklistPatterns;

    // 最小运行时间(秒):过滤掉运行时间过短的进程
    // 0 表示不过滤，默认 20 秒
    int64_t minRunningTimeSeconds = 20;

    // 是否排除内核线程，默认 true
    bool excludeKernelThreads = true;
};

class ProcessEntityRunner : public InputRunner {
public:
    ProcessEntityRunner(const ProcessEntityRunner&) = delete;
    ProcessEntityRunner(ProcessEntityRunner&&) = delete;
    ProcessEntityRunner& operator=(const ProcessEntityRunner&) = delete;
    ProcessEntityRunner& operator=(ProcessEntityRunner&&) = delete;

    static ProcessEntityRunner* GetInstance() {
        static ProcessEntityRunner sInstance;
        return &sInstance;
    }

    void Init() override;
    void Stop() override;
    bool HasRegisteredPlugins() const override;

    // 注册配置
    void RegisterConfig(const std::string& configName,
                        QueueKey processQueueKey,
                        size_t inputIndex,
                        const ProcessFilterConfig& filterConfig = ProcessFilterConfig(),
                        uint32_t incrementalIntervalSeconds = 0,
                        uint32_t fullReportIntervalSeconds = 0);

    // 移除配置
    void RemoveConfig(const std::string& configName);
    void RemoveAllConfigs();

    // 设置全量上报间隔(默认3600秒)
    void SetFullReportInterval(std::chrono::seconds interval);

    // 强制触发一次全量上报(用于测试)
    void TriggerFullReport(const std::string& configName);

    // 执行一次调度（由 TimerEvent 回调）
    void ScheduleOnce(ProcessEntityCollectContextPtr context);

    // 检查任务是否有效
    bool IsCollectTaskValid(std::chrono::steady_clock::time_point startTime, const std::string& configName) const;

private:
    ProcessEntityRunner();
    ~ProcessEntityRunner() override = default;

    // 主采集逻辑（单次执行）
    void CollectProcessEntitiesOnce(ProcessEntityCollectContextPtr context);

    // 全量采集（定期上报所有缓存的进程实体）
    void FullCollect(ProcessEntityCollectContextPtr context);

    // 增量采集（只检测新增/退出/PID复用，不检测属性变化）
    void IncrementalCollect(ProcessEntityCollectContextPtr context);

    // 检测进程变化
    struct ProcessChanges {
        std::vector<ProcessPrimaryKey> added; // 新增进程
        std::vector<ProcessPrimaryKey> removed; // 退出进程
        std::vector<ProcessPrimaryKey> reused; // PID被复用的进程
    };

    ProcessChanges
    DetectChanges(const std::unordered_map<ProcessPrimaryKey, ProcessEntityInfo, ProcessPrimaryKeyHash>& oldCache,
                  const std::unordered_map<pid_t, ProcessPrimaryKey>& currentPidMap);

    // 获取当前所有进程的主key信息
    std::unordered_map<pid_t, ProcessPrimaryKey> GetCurrentProcessPrimaryKeys(time_t now);

    // 获取进程实体信息（包含不变属性和初始值）
    bool GetProcessEntityInfo(pid_t pid, time_t now, ProcessEntityInfo& info);

    // 更新进程可变属性（ppid、user、state等）
    bool UpdateVariableAttributes(pid_t pid, time_t now, ProcessEntityInfo& info);

    // 进程过滤
    bool ShouldCollectProcess(pid_t pid, const ProcessEntityInfo& info, const ProcessFilterConfig& config);

    // 生成进程实体Event（包含不变属性和可变属性）
    void GenerateProcessEntityEvent(PipelineEventGroup* group, const ProcessEntityInfo& info, bool isFullReport);

    // 生成进程关系Event (process -> host)
    void GenerateProcessRelationEvent(PipelineEventGroup* group, const ProcessEntityInfo& info);

    // 推送到队列
    void PushToQueue(ProcessEntityCollectContextPtr context, PipelineEventGroup&& group);

    // 添加主机标签
    void AddHostLabels(PipelineEventGroup& group);

    // 推送下一次 Timer 事件
    void PushNextTimerEvent(ProcessEntityCollectContextPtr context);

    // 初始化指标
    void InitMetrics();

    // 配置运行信息（用于验证任务有效性）
    struct ConfigRunInfo {
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastRunTime;
    };

    mutable std::mutex mConfigMutex;
    std::unordered_map<std::string, ProcessEntityCollectContextPtr> mContexts;
    std::unordered_map<std::string, ConfigRunInfo> mConfigRunInfos;

    // Runner状态
    std::atomic_bool mIsStarted = false;
    std::unique_ptr<ThreadPool> mThreadPool;

    // 全量上报间隔(默认3600秒)
    std::chrono::seconds mFullReportInterval{3600};

    // 增量采集间隔(默认10秒)
    std::chrono::seconds mIncrementalInterval{10};

    // 指标
    MetricsRecordRef mMetricsRecordRef;
    CounterPtr mFullReportTotal;
    CounterPtr mIncrementalReportTotal;
    CounterPtr mProcessAddedTotal;
    CounterPtr mProcessRemovedTotal;
    CounterPtr mProcessReusedTotal;
    IntGaugePtr mCachedProcessCount;
    TimeCounterPtr mCollectLatencyMs;
    TimeCounterPtr mFullCollectLatencyMs;
    TimeCounterPtr mIncrementalCollectLatencyMs;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessEntityRunnerUnittest;
#endif
};

// 采集上下文（类似 HostMonitorContext）
struct ProcessEntityCollectContext {
    // Disable copy constructor and copy assignment
    ProcessEntityCollectContext(const ProcessEntityCollectContext&) = delete;
    ProcessEntityCollectContext& operator=(const ProcessEntityCollectContext&) = delete;

    // Enable move constructor and move assignment
    ProcessEntityCollectContext(ProcessEntityCollectContext&&) = default;
    ProcessEntityCollectContext& operator=(ProcessEntityCollectContext&&) = default;

    std::string mConfigName;
    QueueKey mProcessQueueKey;
    size_t mInputIndex;
    ProcessFilterConfig mFilterConfig;

    std::chrono::seconds mIncrementalInterval; // 增量采集间隔
    std::chrono::seconds mFullReportInterval; // 全量上报间隔

    // 时间管理
    std::chrono::steady_clock::time_point mScheduleTime; // 下次调度时间
    time_t mMetricTime = 0; // 对应的指标时间
    std::chrono::steady_clock::time_point mStartTime; // 配置启动时间（用于验证）

    // 缓存:存储已采集的进程实体信息（永久保存，只在进程退出时清理）
    std::unordered_map<ProcessPrimaryKey, ProcessEntityInfo, ProcessPrimaryKeyHash> processCache;

    // 最后一次全量上报时间
    std::chrono::steady_clock::time_point lastFullReportTime;

    ProcessEntityCollectContext(const std::string& configName,
                                QueueKey processQueueKey,
                                size_t inputIndex,
                                const ProcessFilterConfig& filterConfig,
                                std::chrono::seconds incrementalInterval,
                                std::chrono::seconds fullReportInterval)
        : mConfigName(configName),
          mProcessQueueKey(processQueueKey),
          mInputIndex(inputIndex),
          mFilterConfig(filterConfig),
          mIncrementalInterval(incrementalInterval),
          mFullReportInterval(fullReportInterval),
          mStartTime(std::chrono::steady_clock::now()) {
        lastFullReportTime = mStartTime - fullReportInterval; // 立即触发第一次全量上报
    }

    std::chrono::steady_clock::time_point GetScheduleTime() const { return mScheduleTime; }
    time_t GetMetricTime() const { return mMetricTime; }

    void SetTime(std::chrono::steady_clock::time_point scheduleTime, time_t metricTime) {
        mScheduleTime = scheduleTime;
        mMetricTime = metricTime;
    }

    // 重置时间（首次调度或时钟跳跃后）
    void Reset();

    // 计算首次采集时间（时间对齐）
    void CalculateFirstCollectTime(time_t metricTimeNow, std::chrono::steady_clock::time_point steadyClockNow);

    // 检查系统时钟是否跳跃
    bool CheckClockRolling();
};

} // namespace logtail
