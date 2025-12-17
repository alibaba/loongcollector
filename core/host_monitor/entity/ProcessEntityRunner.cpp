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

#include "host_monitor/entity/ProcessEntityRunner.h"

#include <algorithm>
#include <queue>
#include <regex>
#include <sstream>

#include "common/Flags.h"
#include "common/HashUtil.h"
#include "common/MachineInfoUtil.h"
#include "common/ProcParser.h"
#include "common/StringView.h"
#include "common/TimeUtil.h"
#include "common/timer/Timer.h"
#include "constants/EntityConstants.h"
#include "host_monitor/Constants.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/entity/ProcessEntityTimerEvent.h"
#include "logger/Logger.h"
#include "models/PipelineEventGroup.h"
#include "monitor/AlarmManager.h"
#include "monitor/MetricManager.h"
#include "monitor/Monitor.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "runner/ProcessorRunner.h"

#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif

// 配置flags
DEFINE_FLAG_INT32(process_entity_thread_pool_size, "thread pool size for process entity collection", 2);

namespace logtail {

// ========== ProcessEntityCollectContext 方法实现 ==========

void ProcessEntityCollectContext::Reset() {
    auto systemClockNow = std::chrono::system_clock::now();
    auto steadyClockNow = std::chrono::steady_clock::now();
    auto metricTimeNow = std::chrono::system_clock::to_time_t(systemClockNow);

    CalculateFirstCollectTime(metricTimeNow, steadyClockNow);
}

void ProcessEntityCollectContext::CalculateFirstCollectTime(time_t metricTimeNow,
                                                            std::chrono::steady_clock::time_point steadyClockNow) {
    // 时间对齐：对齐到下一个采集间隔的整数倍
    auto firstMetricTime = ((metricTimeNow / mIncrementalInterval.count()) + 1) * mIncrementalInterval.count();
    auto firstScheduleTime = steadyClockNow + std::chrono::seconds(firstMetricTime - metricTimeNow);

    SetTime(firstScheduleTime, firstMetricTime);

    LOG_INFO(sLogger,
             ("ProcessEntityCollectContext", "CalculateFirstCollectTime")("config", mConfigName)(
                 "metricTimeNow", metricTimeNow)("firstMetricTime", firstMetricTime)("delay_seconds",
                                                                                     firstMetricTime - metricTimeNow));
}

bool ProcessEntityCollectContext::CheckClockRolling() {
    auto steadyClockNow = std::chrono::steady_clock::now();
    auto systemClockNow = std::chrono::system_clock::now();
    auto systemTimeT = std::chrono::system_clock::to_time_t(systemClockNow);

    // 如果 schedule time 和 steady clock 的差值与 metric time 和 system time 的差值相差超过 60 秒
    // 说明系统时钟发生了跳跃
    auto expectedDiff = std::chrono::duration_cast<std::chrono::seconds>(mScheduleTime - steadyClockNow).count();
    auto actualDiff = mMetricTime - systemTimeT;

    if (std::abs(expectedDiff - actualDiff) > 60) {
        LOG_ERROR(
            sLogger,
            ("ProcessEntity system clock rolling",
             "will reset collect scheduling")("config", mConfigName)("original metric time", mMetricTime)(
                "original schedule time", mScheduleTime.time_since_epoch().count())("current system time", systemTimeT)(
                "current steady time",
                steadyClockNow.time_since_epoch().count())("expected diff", expectedDiff)("actual diff", actualDiff));

        AlarmManager::GetInstance()->SendAlarmError(PROCESS_ENTITY_ALARM,
                                                    "ProcessEntity system clock rolling, rolling interval: "
                                                        + std::to_string(expectedDiff - actualDiff)
                                                        + ", config: " + mConfigName,
                                                    "",
                                                    "",
                                                    mConfigName);
        return true;
    }
    return false;
}

// ========== ProcessEntityRunner 方法实现 ==========

ProcessEntityRunner::ProcessEntityRunner()
    : mFullReportInterval(std::chrono::seconds(3600)), // 默认全量上报间隔：1小时
      mIncrementalInterval(std::chrono::seconds(10)) { // 默认增量采集间隔：10秒
}

void ProcessEntityRunner::Init() {
    if (mIsStarted.exchange(true)) {
        LOG_WARNING(sLogger, ("ProcessEntityRunner already started", "skip init"));
        return;
    }

    LOG_INFO(sLogger,
             ("ProcessEntityRunner initializing", "")("full_report_interval", mFullReportInterval.count())(
                 "incremental_interval", mIncrementalInterval.count()));

    // 初始化 Timer（必须在推送事件之前）
    Timer::GetInstance()->Init();

    // 初始化线程池
    mThreadPool = std::make_unique<ThreadPool>(INT32_FLAG(process_entity_thread_pool_size));
    mThreadPool->Start();

    // 初始化指标
    InitMetrics();

    LOG_INFO(sLogger, ("ProcessEntityRunner initialized successfully", ""));
}

void ProcessEntityRunner::Stop() {
    if (!mIsStarted.exchange(false)) {
        return;
    }

    LOG_INFO(sLogger, ("ProcessEntityRunner stopping", ""));

    // 停止线程池
    if (mThreadPool) {
        mThreadPool.reset();
    }

    // 清理配置
    {
        std::lock_guard<std::mutex> lock(mConfigMutex);
        mContexts.clear();
        mConfigRunInfos.clear();
    }

    LOG_INFO(sLogger, ("ProcessEntityRunner stopped", ""));
}

bool ProcessEntityRunner::HasRegisteredPlugins() const {
    std::lock_guard<std::mutex> lock(mConfigMutex);
    return !mContexts.empty();
}

void ProcessEntityRunner::RegisterConfig(const std::string& configName,
                                         QueueKey processQueueKey,
                                         size_t inputIndex,
                                         const ProcessFilterConfig& filterConfig,
                                         uint32_t incrementalIntervalSeconds,
                                         uint32_t fullReportIntervalSeconds) {
    std::lock_guard<std::mutex> lock(mConfigMutex);

    auto it = mContexts.find(configName);
    if (it != mContexts.end()) {
        LOG_WARNING(sLogger, ("config already registered", configName)("will update", ""));
    }

    // 使用配置参数或默认值
    auto incrementalInterval
        = incrementalIntervalSeconds > 0 ? std::chrono::seconds(incrementalIntervalSeconds) : mIncrementalInterval;
    auto fullReportInterval
        = fullReportIntervalSeconds > 0 ? std::chrono::seconds(fullReportIntervalSeconds) : mFullReportInterval;

    // 创建采集上下文
    auto context = std::make_shared<ProcessEntityCollectContext>(
        configName, processQueueKey, inputIndex, filterConfig, incrementalInterval, fullReportInterval);

    // 计算首次采集时间（时间对齐到下一个增量间隔的整数倍）
    // 注意：首次采集时仍会触发全量上报，因为构造函数中设置了 lastFullReportTime = mStartTime - fullReportInterval
    context->Reset();

    // 保存到map
    mContexts[configName] = context;

    // 保存运行信息
    ConfigRunInfo runInfo;
    runInfo.startTime = context->mStartTime;
    runInfo.lastRunTime = context->mStartTime;
    mConfigRunInfos[configName] = runInfo;

    // 创建并推送第一个 Timer 事件
    auto event = std::make_unique<ProcessEntityTimerEvent>(context);
    Timer::GetInstance()->PushEvent(std::move(event));

    LOG_INFO(
        sLogger,
        ("registered process entity config", configName)("exclude_kernel_threads", filterConfig.excludeKernelThreads)(
            "min_running_time_seconds", filterConfig.minRunningTimeSeconds)(
            "incremental_interval", incrementalInterval.count())("full_report_interval", fullReportInterval.count())(
            "first_schedule_time", context->GetScheduleTime().time_since_epoch().count()));
}

void ProcessEntityRunner::RemoveConfig(const std::string& configName) {
    std::lock_guard<std::mutex> lock(mConfigMutex);
    auto it = mContexts.find(configName);
    if (it != mContexts.end()) {
        mContexts.erase(it);
        mConfigRunInfos.erase(configName);
        LOG_INFO(sLogger, ("removed process entity config", configName));
    }
}

void ProcessEntityRunner::RemoveAllConfigs() {
    std::lock_guard<std::mutex> lock(mConfigMutex);
    mContexts.clear();
    mConfigRunInfos.clear();
    LOG_INFO(sLogger, ("removed all process entity configs", ""));
}

void ProcessEntityRunner::SetFullReportInterval(std::chrono::seconds interval) {
    mFullReportInterval = interval;
    LOG_INFO(sLogger, ("updated full report interval", interval.count()));
}

void ProcessEntityRunner::TriggerFullReport(const std::string& configName) {
    std::lock_guard<std::mutex> lock(mConfigMutex);
    auto it = mContexts.find(configName);
    if (it != mContexts.end()) {
        it->second->lastFullReportTime = std::chrono::steady_clock::now() - mFullReportInterval;
        LOG_INFO(sLogger, ("triggered full report for config", configName));
    }
}

// 由 TimerEvent 回调执行一次调度
void ProcessEntityRunner::ScheduleOnce(ProcessEntityCollectContextPtr context) {
    if (!mIsStarted) {
        LOG_WARNING(sLogger, ("ScheduleOnce skipped", "runner not started")("config", context->mConfigName));
        return;
    }

    auto startTime = std::chrono::steady_clock::now();

    // 在线程池中执行采集任务
    auto collectFn = [this, context, startTime]() {
        // 执行采集
        this->CollectProcessEntitiesOnce(context);

        // 更新指标
        auto endTime = std::chrono::steady_clock::now();
        auto latencyNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
        if (mCollectLatencyMs) {
            mCollectLatencyMs->Add(latencyNs);
        }

        // 检查配置是否仍然有效
        {
            std::lock_guard<std::mutex> lock(mConfigMutex);
            auto it = mConfigRunInfos.find(context->mConfigName);
            if (it == mConfigRunInfos.end() || it->second.startTime != context->mStartTime) {
                return;
            }
            it->second.lastRunTime = std::chrono::steady_clock::now();
        }

        // 推送下一次 Timer 事件
        this->PushNextTimerEvent(context);
    };

    mThreadPool->Add(collectFn);
}

// 检查任务是否有效
bool ProcessEntityRunner::IsCollectTaskValid(std::chrono::steady_clock::time_point startTime,
                                             const std::string& configName) const {
    std::lock_guard<std::mutex> lock(mConfigMutex);
    auto it = mConfigRunInfos.find(configName);
    if (it == mConfigRunInfos.end()) {
        return false;
    }
    return it->second.startTime == startTime;
}

// 推送下一次 Timer 事件
void ProcessEntityRunner::PushNextTimerEvent(ProcessEntityCollectContextPtr context) {
    // 检查系统时钟是否跳跃
    if (context->CheckClockRolling()) {
        context->Reset();
    } else {
        auto now = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point nextScheduleTime
            = context->GetScheduleTime() + context->mIncrementalInterval;
        time_t nextMetricTime = context->GetMetricTime() + context->mIncrementalInterval.count();

        int64_t skipCount = 0;
        if (now > nextScheduleTime) {
            // 延迟过大，计算跳过的次数
            skipCount = (now - nextScheduleTime) / context->mIncrementalInterval;
            nextScheduleTime += (skipCount + 1) * context->mIncrementalInterval;
            nextMetricTime += (skipCount + 1) * context->mIncrementalInterval.count();

            LOG_WARNING(sLogger,
                        ("ProcessEntity skip collect",
                         "may cause data loss")("config", context->mConfigName)("skip count", skipCount + 1));
        }

        context->SetTime(nextScheduleTime, nextMetricTime);
    }

    // 创建并推送 Timer 事件
    auto event = std::make_unique<ProcessEntityTimerEvent>(context);
    Timer::GetInstance()->PushEvent(std::move(event));
}

void ProcessEntityRunner::CollectProcessEntitiesOnce(ProcessEntityCollectContextPtr context) {
    if (!mIsStarted) {
        LOG_WARNING(sLogger,
                    ("CollectProcessEntitiesOnce skipped", "runner not started")("config", context->mConfigName));
        return;
    }

    auto now = std::chrono::steady_clock::now();

    // 判断是否需要全量上报
    bool needFullReport = (now - context->lastFullReportTime) >= context->mFullReportInterval;

    if (needFullReport) {
        LOG_INFO(sLogger, ("performing full collect", context->mConfigName));
        FullCollect(context);
        context->lastFullReportTime = now;
        ADD_COUNTER(mFullReportTotal, 1);
    } else {
        LOG_INFO(sLogger, ("performing incremental collect", context->mConfigName));
        // 增量采集
        IncrementalCollect(context);
        ADD_COUNTER(mIncrementalReportTotal, 1);
    }

    SET_GAUGE(mCachedProcessCount, context->processCache.size());
}

void ProcessEntityRunner::FullCollect(ProcessEntityCollectContextPtr context) {
    auto& cache = context->processCache;
    const auto& filterConfig = context->mFilterConfig;

    time_t now = time(nullptr);

    // 获取当前所有进程的主key
    auto currentPidMap = GetCurrentProcessPrimaryKeys(now);

    // 第一步：收集所有符合基本过滤条件的进程
    std::vector<std::pair<ProcessPrimaryKey, ProcessEntityInfo>> candidateProcesses;

    for (const auto& [pid, primaryKey] : currentPidMap) {
        ProcessEntityInfo info;

        // 尝试从缓存获取
        auto cacheIt = cache.find(primaryKey);
        if (cacheIt != cache.end()) {
            // 缓存命中：使用缓存的不变属性
            info = cacheIt->second;

            // 全量上报时更新可变属性（ppid、user、state）
            UpdateVariableAttributes(pid, now, info);

            info.lastReportTime = now;
        } else {
            // 新进程，获取实体信息（包含不变属性和初始值）
            if (!GetProcessEntityInfo(pid, now, info)) {
                continue;
            }
            info.firstObservedTime = now;
            info.lastReportTime = now;
            info.lastVariableUpdateTime = now;
        }

        // 进程过滤
        if (!ShouldCollectProcess(pid, info, filterConfig)) {
            continue;
        }

        candidateProcesses.emplace_back(primaryKey, info);
    }

    // 生成事件并更新缓存
    PipelineEventGroup group(std::make_shared<SourceBuffer>());

    for (const auto& [primaryKey, info] : candidateProcesses) {
        // 生成事件（包含不变属性和可变属性）
        GenerateProcessEntityEvent(&group, info, true);

        // 更新缓存（永久保存，直到进程退出）
        cache[primaryKey] = info;
    }

    // 清理已退出的进程
    std::vector<ProcessPrimaryKey> toRemove;
    for (const auto& [key, detail] : cache) {
        if (currentPidMap.find(key.pid) == currentPidMap.end()) {
            toRemove.push_back(key);
        }
    }

    for (const auto& key : toRemove) {
        cache.erase(key);
        ADD_COUNTER(mProcessRemovedTotal, 1);
    }

    // 推送到队列
    if (group.GetEvents().size() > 0) {
        AddHostLabels(group);
        PushToQueue(context, std::move(group));
    }

    LOG_DEBUG(sLogger,
              ("full collect completed", context->mConfigName)("total_processes", currentPidMap.size())(
                  "candidates_after_filter",
                  candidateProcesses.size())("cached_processes", cache.size())("removed_processes", toRemove.size()));
}

void ProcessEntityRunner::IncrementalCollect(ProcessEntityCollectContextPtr context) {
    auto& cache = context->processCache;
    const auto& filterConfig = context->mFilterConfig;

    time_t now = time(nullptr);

    // 获取当前所有进程的主key
    auto currentPidMap = GetCurrentProcessPrimaryKeys(now);

    // 检测变化
    auto changes = DetectChanges(cache, currentPidMap);

    if (changes.added.empty() && changes.removed.empty() && changes.reused.empty()) {
        // 没有变化,不生成事件
        return;
    }

    LOG_DEBUG(sLogger,
              ("incremental collect", context->mConfigName)("added", changes.added.size())(
                  "removed", changes.removed.size())("reused", changes.reused.size()));

    // 创建PipelineEventGroup
    PipelineEventGroup group(std::make_shared<SourceBuffer>());

    // 第一步：收集新增进程的候选列表（应用基本过滤）
    std::vector<std::pair<ProcessPrimaryKey, ProcessEntityInfo>> candidateProcesses;

    // 处理新增进程（获取实体信息）
    for (const auto& key : changes.added) {
        ProcessEntityInfo info;
        if (!GetProcessEntityInfo(key.pid, now, info)) {
            continue;
        }

        info.firstObservedTime = now;
        info.lastReportTime = now;

        // 进程过滤
        if (!ShouldCollectProcess(key.pid, info, filterConfig)) {
            continue;
        }

        // 生成增量事件（实体属性不变，后续只需全量定期上报）
        GenerateProcessEntityEvent(&group, info, false);

        // 加入缓存（永久保存，直到进程退出）
        cache[key] = info;
        ADD_COUNTER(mProcessAddedTotal, 1);
    }

    // 处理PID复用（旧进程退出，新进程复用了相同PID但startTime不同）
    for (const auto& key : changes.reused) {
        ProcessEntityInfo info;
        if (!GetProcessEntityInfo(key.pid, now, info)) {
            continue;
        }

        info.firstObservedTime = now;
        info.lastReportTime = now;

        // 进程过滤
        if (!ShouldCollectProcess(key.pid, info, filterConfig)) {
            continue;
        }

        // 生成增量事件
        GenerateProcessEntityEvent(&group, info, false);

        // 更新缓存（覆盖旧进程，存储新进程）
        cache[key] = info;
        ADD_COUNTER(mProcessReusedTotal, 1);
    }

    // 处理退出进程
    for (const auto& key : changes.removed) {
        cache.erase(key);
        ADD_COUNTER(mProcessRemovedTotal, 1);
    }

    // 推送到队列
    if (group.GetEvents().size() > 0) {
        AddHostLabels(group);
        PushToQueue(context, std::move(group));
    }
}

ProcessEntityRunner::ProcessChanges ProcessEntityRunner::DetectChanges(
    const std::unordered_map<ProcessPrimaryKey, ProcessEntityInfo, ProcessPrimaryKeyHash>& oldCache,
    const std::unordered_map<pid_t, ProcessPrimaryKey>& currentPidMap) {
    ProcessChanges changes;

    // 构建旧的PID映射
    std::unordered_map<pid_t, ProcessPrimaryKey> oldPidMap;
    for (const auto& [key, detail] : oldCache) {
        oldPidMap[key.pid] = key;
    }

    // 检测新增和PID复用
    for (const auto& [pid, currentKey] : currentPidMap) {
        auto it = oldPidMap.find(pid);
        if (it == oldPidMap.end()) {
            // 新增进程：PID不在缓存中
            changes.added.push_back(currentKey);
        } else if (it->second.startTime != currentKey.startTime) {
            // PID被复用：PID相同但startTime不同，说明是不同的进程实例
            changes.reused.push_back(currentKey);
            // 旧进程已退出，标记为移除
            changes.removed.push_back(it->second);
        }
        // else: PID和startTime都相同 → 同一进程实例，实体属性不变，无需处理
    }

    // 检测退出进程
    for (const auto& [pid, oldKey] : oldPidMap) {
        auto it = currentPidMap.find(pid);
        if (it == currentPidMap.end()) {
            // 进程已退出
            changes.removed.push_back(oldKey);
        }
    }

    return changes;
}

std::unordered_map<pid_t, ProcessPrimaryKey> ProcessEntityRunner::GetCurrentProcessPrimaryKeys(time_t now) {
    std::unordered_map<pid_t, ProcessPrimaryKey> pidMap;

    // 获取进程列表(复用SystemInterface的缓存)
    ProcessListInformation processListInfo;
    if (!SystemInterface::GetInstance()->GetProcessListInformation(now, processListInfo)) {
        LOG_ERROR(sLogger, ("failed to get process list", ""));
        return pidMap;
    }

    // 获取每个进程的CPU信息(包含startTime)
    for (pid_t pid : processListInfo.pids) {
        ProcessInformation processInfo;
        if (!SystemInterface::GetInstance()->GetProcessInformation(now, pid, processInfo)) {
            continue;
        }

        ProcessPrimaryKey key;
        key.pid = pid;
        key.startTime = processInfo.stat.startTicks;

        pidMap[pid] = key;
    }

    return pidMap;
}

bool ProcessEntityRunner::GetProcessEntityInfo(pid_t pid, time_t now, ProcessEntityInfo& info) {
    // 获取基本stat信息（不变属性）
    ProcessInformation processInfo;
    if (!SystemInterface::GetInstance()->GetProcessInformation(now, pid, processInfo)) {
        return false;
    }

    info.pid = pid;
    info.flags = processInfo.stat.flags; // 进程标志位（不变，用于判断内核线程）
    info.startTime = processInfo.stat.startTicks; // 启动时间ticks（不变）
    info.comm = processInfo.stat.name; // 进程名（不变）

    // 可变属性：父进程ID、状态
    info.ppid = processInfo.stat.parentPid;
    info.state = processInfo.stat.state;
    info.isZombie = (processInfo.stat.state == 'Z');

    // 计算进程启动的Unix时间戳（用于计算运行时间）
    SystemInformation systemInfo;
    if (SystemInterface::GetInstance()->GetSystemInformation(systemInfo)) {
        info.startTimeUnix = systemInfo.bootTime + (info.startTime / SYSTEM_HERTZ);
    }

    // 获取cmdline（不变属性：进程启动时确定）
    ProcessCmdlineString cmdlineStr;
    if (SystemInterface::GetInstance()->GetProcessCmdlineString(now, pid, cmdlineStr)) {
        if (!cmdlineStr.cmdline.empty()) {
            info.cmdline = cmdlineStr.cmdline[0];
            // 解析参数
            std::istringstream iss(info.cmdline);
            std::string arg;
            while (std::getline(iss, arg, '\0')) {
                if (!arg.empty()) {
                    info.args.push_back(arg);
                }
            }
        }
    }

    // 获取可执行文件路径（不变属性：/proc/[pid]/exe符号链接）
    ProcessExecutePath exePath;
    if (SystemInterface::GetInstance()->GetExecutablePathCache(now, pid, exePath)) {
        info.exe = exePath.path;
    }

    // 获取用户信息（可变属性，性能代价：+0.1ms/进程）
    ProcessCredName credName;
    if (SystemInterface::GetInstance()->GetProcessCredNameObj(now, pid, credName)) {
        info.user = credName.user;
    }

    return true;
}

bool ProcessEntityRunner::UpdateVariableAttributes(pid_t pid, time_t now, ProcessEntityInfo& info) {
    // 更新可变属性：ppid、user、state
    // 此方法仅在全量采集时调用，不追踪变化

    // 1. 更新 ppid 和 state（从 /proc/[pid]/stat）
    ProcessInformation processInfo;
    if (SystemInterface::GetInstance()->GetProcessInformation(now, pid, processInfo)) {
        info.ppid = processInfo.stat.parentPid;
        info.state = processInfo.stat.state;
        info.isZombie = (processInfo.stat.state == 'Z');
    }

    // 2. 更新用户（从 /proc/[pid]/status）
    ProcessCredName credName;
    if (SystemInterface::GetInstance()->GetProcessCredNameObj(now, pid, credName)) {
        info.user = credName.user;
    }

    info.lastVariableUpdateTime = now;
    return true;
}

bool ProcessEntityRunner::ShouldCollectProcess(pid_t pid,
                                               const ProcessEntityInfo& info,
                                               const ProcessFilterConfig& config) {
    // 过滤内核线程
    if (config.excludeKernelThreads) {
        // 检查 PF_KTHREAD 标志位（0x00200000）
        // 内核线程在进程创建时就设置了该标志位，这是最准确的判断方法
        // See: https://github.com/torvalds/linux/commit/7b34e4283c685f5cc6ba6d30e939906eee0d4bcf
        if (ProcParser::IsKernelThread(info.flags)) {
            return false;
        }
    }

    // 最小运行时间过滤（0 表示不过滤）
    if (config.minRunningTimeSeconds > 0) {
        time_t now = time(nullptr);
        SystemInformation systemInfo;
        if (SystemInterface::GetInstance()->GetSystemInformation(systemInfo)) {
            // 计算进程运行时间
            int64_t startTimeSec = info.startTime / SYSTEM_HERTZ + systemInfo.bootTime;
            int64_t runningTime = now - startTimeSec;
            if (runningTime < config.minRunningTimeSeconds) {
                return false;
            }
        }
    }

    // 黑名单过滤
    for (const auto& pattern : config.blacklistPatterns) {
        try {
            std::regex re(pattern);
            if (std::regex_search(info.comm, re) || std::regex_search(info.exe, re)) {
                return false;
            }
        } catch (const std::regex_error& e) {
            LOG_WARNING(sLogger, ("invalid blacklist regex pattern", pattern)("error", e.what()));
        }
    }

    // 白名单过滤（如果配置了白名单，则只采集匹配的）
    if (!config.whitelistPatterns.empty()) {
        bool matched = false;
        for (const auto& pattern : config.whitelistPatterns) {
            try {
                std::regex re(pattern);
                if (std::regex_search(info.comm, re) || std::regex_search(info.exe, re)) {
                    matched = true;
                    break;
                }
            } catch (const std::regex_error& e) {
                LOG_WARNING(sLogger, ("invalid whitelist regex pattern", pattern)("error", e.what()));
            }
        }
        if (!matched) {
            return false;
        }
    }


    return true;
}


void ProcessEntityRunner::GenerateProcessEntityEvent(PipelineEventGroup* group,
                                                     const ProcessEntityInfo& info,
                                                     bool isFullReport) {
    auto* event = group->AddLogEvent();
    // 获取包含纳秒的时间戳
    LogtailTime currentTime = GetCurrentLogtailTime();
    event->SetTimestamp(currentTime.tv_sec, static_cast<uint32_t>(currentTime.tv_nsec));

    // 获取系统信息用于计算进程启动时间
    SystemInformation systemInfo;
    if (!SystemInterface::GetInstance()->GetSystemInformation(systemInfo)) {
        LOG_ERROR(sLogger, ("failed to get system information", ""));
        return;
    }

    // 使用预先计算好的进程启动Unix时间戳
    std::string processCreateTime = std::to_string(info.startTimeUnix);

    // 主键（不变）
    event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_PID, std::to_string(info.pid));
    event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_KTIME, processCreateTime);

    // 不变属性
    event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_COMM, info.comm);

    if (!info.exe.empty()) {
        event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_BINARY, info.exe);
    }

    if (!info.cmdline.empty()) {
        event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_ARGUMENTS, info.cmdline);
    }

    // 可变属性（全量采集时重新读取）
    event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_PPID, std::to_string(info.ppid));

    if (!info.user.empty()) {
        event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_USER, info.user);
    }

    if (info.state != '\0') {
        event->SetContent("state", std::string(1, info.state));
    }


    // 运行时间（秒）- 实时计算
    int64_t runningTimeSeconds = currentTime.tv_sec - info.startTimeUnix;
    if (runningTimeSeconds > 0) {
        event->SetContent("runtime_seconds", std::to_string(runningTimeSeconds));
    }
}


void ProcessEntityRunner::AddHostLabels(PipelineEventGroup& group) {
#ifdef __ENTERPRISE__
    const auto* entity = InstanceIdentity::Instance()->GetEntity();
    if (entity != nullptr) {
        auto hostID = group.GetSourceBuffer()->CopyString(entity->GetHostID());
        auto userID = group.GetSourceBuffer()->CopyString(entity->GetEcsUserID());
        auto hostName = group.GetSourceBuffer()->CopyString(LoongCollectorMonitor::mHostname);
        for (auto& e : group.MutableEvents()) {
            if (e.Is<LogEvent>()) {
                auto& logEvent = e.Cast<LogEvent>();
                logEvent.SetContentNoCopy(StringView(DEFAULT_INSTANCE_ID_LABEL), StringView(hostID.data, hostID.size));
                logEvent.SetContentNoCopy(StringView(DEFAULT_USER_ID_LABEL), StringView(userID.data, userID.size));
                logEvent.SetContentNoCopy(StringView(DEFAULT_HOST_NAME_LABEL),
                                          StringView(hostName.data, hostName.size));
            }
        }
    }
#else
    auto hostIP = group.GetSourceBuffer()->CopyString(LoongCollectorMonitor::mIpAddr);
    auto hostName = group.GetSourceBuffer()->CopyString(LoongCollectorMonitor::mHostname);
    for (auto& e : group.MutableEvents()) {
        if (e.Is<LogEvent>()) {
            auto& logEvent = e.Cast<LogEvent>();
            logEvent.SetContentNoCopy(StringView(DEFAULT_HOST_IP_LABEL), StringView(hostIP.data, hostIP.size));
            logEvent.SetContentNoCopy(StringView(DEFAULT_HOST_NAME_LABEL), StringView(hostName.data, hostName.size));
        }
    }
#endif
}

void ProcessEntityRunner::PushToQueue(ProcessEntityCollectContextPtr context, PipelineEventGroup&& group) {
    std::lock_guard<std::mutex> lock(mConfigMutex);
    auto it = mConfigRunInfos.find(context->mConfigName);
    if (it == mConfigRunInfos.end() || it->second.startTime != context->mStartTime) {
        // 配置已移除或已更新，丢弃数据
        LOG_WARNING(sLogger, ("PushToQueue skipped", "config not found or outdated")("config", context->mConfigName));
        return;
    }

    size_t eventCount = group.GetEvents().size();

    // 推送到 ProcessorRunner
    bool pushResult
        = ProcessorRunner::GetInstance()->PushQueue(context->mProcessQueueKey, context->mInputIndex, std::move(group));

    if (!pushResult) {
        LOG_ERROR(sLogger,
                  ("ProcessEntity push process queue failed",
                   "discard data")("config", context->mConfigName)("event_count", eventCount));
    }
}


void ProcessEntityRunner::InitMetrics() {
    MetricLabels labels;
    labels.emplace_back(METRIC_LABEL_KEY_RUNNER_NAME, "process_entity_runner");
    WriteMetrics::GetInstance()->CreateMetricsRecordRef(
        mMetricsRecordRef, MetricCategory::METRIC_CATEGORY_RUNNER, std::move(labels));

    mFullReportTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_FULL_REPORT_TOTAL);
    mIncrementalReportTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_INCREMENTAL_REPORT_TOTAL);
    mProcessAddedTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_PROCESS_ADDED_TOTAL);
    mProcessRemovedTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_PROCESS_REMOVED_TOTAL);
    mProcessReusedTotal = mMetricsRecordRef.CreateCounter(METRIC_RUNNER_PROCESS_REUSED_TOTAL);
    mCachedProcessCount = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_CACHED_PROCESS_COUNT);
    mCollectLatencyMs = mMetricsRecordRef.CreateTimeCounter(METRIC_RUNNER_COLLECT_LATENCY_MS);

    WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricsRecordRef);
}

} // namespace logtail
