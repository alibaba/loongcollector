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

#include "host_monitor/collector/ProcessEntityCollector.h"

#include <regex>

#include "common/ProcParser.h"
#include "common/StringView.h"
#include "common/TimeUtil.h"
#include "constants/EntityConstants.h"
#include "host_monitor/Constants.h"
#include "host_monitor/SystemInterface.h"
#include "logger/Logger.h"
#include "models/PipelineEventGroup.h"
#include "monitor/MetricManager.h"
#include "monitor/Monitor.h"
#include "monitor/metric_constants/MetricConstants.h"

#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif

namespace logtail {

const std::string ProcessEntityCollector::sName = "process_entity";

// 静态成员初始化
std::mutex ProcessEntityCollector::sConfigMutex;
std::unordered_map<std::string, ProcessEntityCollector::GlobalConfig> ProcessEntityCollector::sConfigs;

// 静态 Metrics 初始化
std::once_flag ProcessEntityCollector::sMetricsInitFlag;
MetricsRecordRef ProcessEntityCollector::sMetricsRecordRef;
CounterPtr ProcessEntityCollector::sFullReportTotal;
CounterPtr ProcessEntityCollector::sIncrementalReportTotal;
CounterPtr ProcessEntityCollector::sProcessAddedTotal;
CounterPtr ProcessEntityCollector::sProcessRemovedTotal;
CounterPtr ProcessEntityCollector::sProcessReusedTotal;
IntGaugePtr ProcessEntityCollector::sCachedProcessCount;
TimeCounterPtr ProcessEntityCollector::sCollectLatencyMs;
TimeCounterPtr ProcessEntityCollector::sFullCollectLatencyMs;
TimeCounterPtr ProcessEntityCollector::sIncrementalCollectLatencyMs;

// ========== ProcessFilterConfig 方法实现 ==========

bool ProcessFilterConfig::IsSimplePattern(const std::string& pattern) {
    // 判断模式是否为简单字符串（不包含正则表达式特殊字符）
    // 简单模式可以使用 std::string::find 快速匹配，避免正则引擎开销
    const char* regexSpecialChars = ".*+?[]{}()^$|\\";
    return pattern.find_first_of(regexSpecialChars) == std::string::npos;
}

bool ProcessFilterConfig::CompileRegexes() {
    compiledBlacklistRegexes.clear();
    compiledWhitelistRegexes.clear();
    simpleBlacklistPatterns.clear();
    simpleWhitelistPatterns.clear();

    // 处理黑名单：分离简单模式和复杂模式
    for (const auto& pattern : blacklistPatterns) {
        if (IsSimplePattern(pattern)) {
            // 简单模式：使用字符串查找
            simpleBlacklistPatterns.push_back(pattern);
        } else {
            // 复杂模式：编译为正则表达式
            try {
                compiledBlacklistRegexes.emplace_back(pattern);
            } catch (const std::regex_error& e) {
                LOG_ERROR(sLogger,
                          ("failed to compile blacklist regex pattern", pattern)("error", e.what())("code", e.code()));
                return false;
            }
        }
    }

    // 处理白名单：分离简单模式和复杂模式
    for (const auto& pattern : whitelistPatterns) {
        if (IsSimplePattern(pattern)) {
            // 简单模式：使用字符串查找
            simpleWhitelistPatterns.push_back(pattern);
        } else {
            // 复杂模式：编译为正则表达式
            try {
                compiledWhitelistRegexes.emplace_back(pattern);
            } catch (const std::regex_error& e) {
                LOG_ERROR(sLogger,
                          ("failed to compile whitelist regex pattern", pattern)("error", e.what())("code", e.code()));
                return false;
            }
        }
    }

    LOG_INFO(
        sLogger,
        ("compiled filter patterns",
         "")("blacklist_simple", simpleBlacklistPatterns.size())("blacklist_regex", compiledBlacklistRegexes.size())(
            "whitelist_simple", simpleWhitelistPatterns.size())("whitelist_regex", compiledWhitelistRegexes.size()));

    return true;
}

// ========== ProcessEntityCollector 方法实现 ==========

ProcessEntityCollector::ProcessEntityCollector() : mIsFirstCollect(true) {
    // 设置初始时间为很久之前，以便立即触发第一次全量上报
    constexpr std::chrono::seconds kDefaultFullReportInterval{3600}; // 1小时
    mLastFullReportTime = std::chrono::steady_clock::now() - kDefaultFullReportInterval;
}

// 静态方法：注册全局配置
void ProcessEntityCollector::RegisterConfig(const std::string& configName,
                                            const ProcessFilterConfig& filterConfig,
                                            std::chrono::seconds fullReportInterval) {
    std::lock_guard<std::mutex> lock(sConfigMutex);
    GlobalConfig config;
    config.filterConfig = filterConfig;
    config.fullReportInterval = fullReportInterval;
    config.forceFullReport = false;
    sConfigs[configName] = config;

    LOG_INFO(sLogger,
             ("ProcessEntityCollector registered global config", configName)("full_report_interval",
                                                                             fullReportInterval.count()));
}

// 静态方法：注销全局配置
void ProcessEntityCollector::UnregisterConfig(const std::string& configName) {
    std::lock_guard<std::mutex> lock(sConfigMutex);
    sConfigs.erase(configName);
    LOG_INFO(sLogger, ("ProcessEntityCollector unregistered global config", configName));
}

// 静态方法：触发全量上报
void ProcessEntityCollector::TriggerFullReport(const std::string& configName) {
    std::lock_guard<std::mutex> lock(sConfigMutex);
    auto it = sConfigs.find(configName);
    if (it != sConfigs.end()) {
        it->second.forceFullReport = true;
        LOG_INFO(sLogger, ("ProcessEntityCollector triggered full report", configName));
    }
}

bool ProcessEntityCollector::Init(HostMonitorContext& collectContext) {
    mConfigName = collectContext.mConfigName;

    // 初始化指标（线程安全，只执行一次）
    std::call_once(sMetricsInitFlag, &ProcessEntityCollector::InitMetrics);

    // 从全局配置加载过滤配置
    {
        std::lock_guard<std::mutex> lock(sConfigMutex);
        auto it = sConfigs.find(mConfigName);
        if (it != sConfigs.end()) {
            mFilterConfig = it->second.filterConfig;
            LOG_INFO(sLogger,
                     ("ProcessEntityCollector initializing",
                      "")("config", mConfigName)("full_report_interval", it->second.fullReportInterval.count()));
        } else {
            LOG_WARNING(sLogger, ("ProcessEntityCollector Init: config not found, using defaults", mConfigName));
        }
    }

    return BaseCollector::Init(collectContext);
}

bool ProcessEntityCollector::Collect(HostMonitorContext& collectContext, PipelineEventGroup* groupPtr) {
    if (groupPtr == nullptr) {
        LOG_WARNING(sLogger, ("ProcessEntityCollector Collect skipped", "groupPtr is null"));
        return false;
    }

    auto collectStartTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    // 合并锁操作：一次性读取全局配置和实例状态，减少锁开销
    bool needFullReport = false;
    {
        // 先获取全局配置锁，再获取实例状态锁（保持一致的加锁顺序）
        std::lock_guard<std::mutex> configLock(sConfigMutex);
        std::lock_guard<std::mutex> stateLock(mStateMutex);

        bool forceFullReport = false;
        std::chrono::seconds currentFullReportInterval{3600}; // 默认值：1小时

        // 读取全局配置
        auto it = sConfigs.find(mConfigName);
        if (it != sConfigs.end()) {
            if (it->second.forceFullReport) {
                forceFullReport = true;
                it->second.forceFullReport = false; // 重置标志
            }
            // 读取最新的全量上报间隔（支持动态修改）
            currentFullReportInterval = it->second.fullReportInterval;
        }

        // 判断是否需要全量上报（在同一锁作用域内完成）
        needFullReport = mIsFirstCollect || forceFullReport || (now - mLastFullReportTime) >= currentFullReportInterval;
    }

    if (needFullReport) {
        LOG_DEBUG(sLogger, ("performing full collect", collectContext.mConfigName));
        FullCollect(collectContext, groupPtr);

        // 更新状态（加锁保护）
        {
            std::lock_guard<std::mutex> lock(mStateMutex);
            mLastFullReportTime = now;
            mIsFirstCollect = false;
        }
        ADD_COUNTER(sFullReportTotal, 1);
    } else {
        LOG_DEBUG(sLogger, ("performing incremental collect", collectContext.mConfigName));
        IncrementalCollect(collectContext, groupPtr);
        ADD_COUNTER(sIncrementalReportTotal, 1);
    }

    // 更新总延迟指标
    auto collectEndTime = std::chrono::steady_clock::now();
    auto latencyNs = std::chrono::duration_cast<std::chrono::nanoseconds>(collectEndTime - collectStartTime);
    if (sCollectLatencyMs) {
        sCollectLatencyMs->Add(latencyNs);
    }

    // 更新缓存进程数
    SET_GAUGE(sCachedProcessCount, mProcessCache.size());

    return true;
}

const std::chrono::seconds ProcessEntityCollector::GetCollectInterval() const {
    // 使用较短的增量采集间隔（默认10秒）
    return std::chrono::seconds(10);
}

void ProcessEntityCollector::FullCollect(HostMonitorContext& collectContext, PipelineEventGroup* groupPtr) {
    auto startTime = std::chrono::steady_clock::now();
    time_t now = time(nullptr);

    // 获取当前所有进程的主key
    auto currentPidMap = GetCurrentProcessPrimaryKeys(now);

    // 直接遍历，生成事件并更新缓存（避免中间存储）
    size_t candidatesCount = 0;

    for (const auto& [pid, primaryKey] : currentPidMap) {
        ProcessEntityInfo info;
        bool isNewProcess = false;

        // 尝试从缓存获取
        auto cacheIt = mProcessCache.find(primaryKey);
        if (cacheIt != mProcessCache.end()) {
            // 缓存命中：拷贝一份以确保异常安全
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
            isNewProcess = true;
        }

        // 进程过滤（可能抛出异常）
        if (!ShouldCollectProcess(pid, info)) {
            continue;
        }

        // 生成事件（可能抛出异常）
        GenerateProcessEntityEvent(groupPtr, info, true);

        // 所有可能失败的操作都成功后，才更新缓存
        if (isNewProcess) {
            // 新进程：插入缓存和索引
            mProcessCache[primaryKey] = std::move(info);
            mPidToKeyIndex[pid] = primaryKey;
        } else {
            // 已有进程：更新缓存（使用 move 避免拷贝）
            cacheIt->second = std::move(info);
        }

        ++candidatesCount;
    }

    // 清理已退出的进程（同步清理缓存和索引）
    std::vector<ProcessPrimaryKey> toRemove;
    for (const auto& [key, detail] : mProcessCache) {
        if (currentPidMap.find(key.pid) == currentPidMap.end()) {
            toRemove.push_back(key);
        }
    }

    for (const auto& key : toRemove) {
        mProcessCache.erase(key);
        mPidToKeyIndex.erase(key.pid);
        ADD_COUNTER(sProcessRemovedTotal, 1);
    }

    // 添加主机标签
    if (!groupPtr->GetEvents().empty()) {
        AddHostLabels(*groupPtr);
    }

    // 记录全量采集延迟
    auto endTime = std::chrono::steady_clock::now();
    auto latencyNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
    if (sFullCollectLatencyMs) {
        sFullCollectLatencyMs->Add(latencyNs);
    }

    LOG_DEBUG(sLogger,
              ("full collect completed", collectContext.mConfigName)("total_processes", currentPidMap.size())(
                  "candidates_after_filter",
                  candidatesCount)("cached_processes", mProcessCache.size())("removed_processes", toRemove.size()));
}

void ProcessEntityCollector::IncrementalCollect(HostMonitorContext& collectContext, PipelineEventGroup* groupPtr) {
    auto startTime = std::chrono::steady_clock::now();
    time_t now = time(nullptr);

    // 获取当前所有进程的主key
    auto currentPidMap = GetCurrentProcessPrimaryKeys(now);

    // 检测变化（使用维护的索引，避免重建）
    auto changes = DetectChanges(mPidToKeyIndex, currentPidMap);

    if (changes.added.empty() && changes.removed.empty() && changes.reused.empty()) {
        // 没有变化,不生成事件
        return;
    }

    LOG_DEBUG(sLogger,
              ("incremental collect", collectContext.mConfigName)("added", changes.added.size())(
                  "removed", changes.removed.size())("reused", changes.reused.size()));

    // 处理新增进程（获取实体信息）
    for (const auto& key : changes.added) {
        ProcessEntityInfo info;
        if (!GetProcessEntityInfo(key.pid, now, info)) {
            continue;
        }

        info.firstObservedTime = now;
        info.lastReportTime = now;

        // 进程过滤
        if (!ShouldCollectProcess(key.pid, info)) {
            continue;
        }

        // 生成增量事件（实体属性不变，后续只需全量定期上报）
        GenerateProcessEntityEvent(groupPtr, info, false);

        // 加入缓存和索引（永久保存，直到进程退出）
        // 使用 emplace 避免默认构造（新增进程，键肯定不存在）
        auto [it, inserted] = mProcessCache.emplace(key, std::move(info));
        if (!inserted) {
            LOG_WARNING(sLogger,
                        ("process already in cache during add, this should not happen",
                         "")("pid", key.pid)("startTime", key.startTime));
        }
        mPidToKeyIndex[key.pid] = key;
        ADD_COUNTER(sProcessAddedTotal, 1);
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
        if (!ShouldCollectProcess(key.pid, info)) {
            continue;
        }

        // 生成增量事件
        GenerateProcessEntityEvent(groupPtr, info, false);

        // 更新缓存和索引（覆盖旧进程，存储新进程）
        // PID 复用时，旧 key (pid, oldStartTime) 和新 key (pid, newStartTime) 不同
        // 因此 emplace 应该成功插入新键
        auto [it, inserted] = mProcessCache.emplace(key, std::move(info));
        if (!inserted) {
            LOG_WARNING(sLogger,
                        ("process already in cache during reuse, this should not happen",
                         "")("pid", key.pid)("startTime", key.startTime));
        }
        mPidToKeyIndex[key.pid] = key;
        ADD_COUNTER(sProcessReusedTotal, 1);
    }

    // 处理退出进程（同步清理缓存和索引）
    for (const auto& key : changes.removed) {
        mProcessCache.erase(key);
        mPidToKeyIndex.erase(key.pid);
        ADD_COUNTER(sProcessRemovedTotal, 1);
    }

    // 添加主机标签
    if (!groupPtr->GetEvents().empty()) {
        AddHostLabels(*groupPtr);
    }

    // 记录增量采集延迟
    auto endTime = std::chrono::steady_clock::now();
    auto latencyNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
    if (sIncrementalCollectLatencyMs) {
        sIncrementalCollectLatencyMs->Add(latencyNs);
    }
}

ProcessEntityCollector::ProcessChanges
ProcessEntityCollector::DetectChanges(const std::unordered_map<pid_t, ProcessPrimaryKey>& oldPidMap,
                                      const std::unordered_map<pid_t, ProcessPrimaryKey>& currentPidMap) {
    ProcessChanges changes;

    // 检测新增和PID复用（直接使用传入的索引，无需重建）
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

std::unordered_map<pid_t, ProcessPrimaryKey> ProcessEntityCollector::GetCurrentProcessPrimaryKeys(time_t now) {
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

bool ProcessEntityCollector::GetProcessEntityInfo(pid_t pid, time_t now, ProcessEntityInfo& info) {
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
    // 注意：/proc/[pid]/cmdline 使用 '\0' 分隔参数，GetProcessCmdlineString 读取整个文件作为一行
    ProcessCmdlineString cmdlineStr;
    if (SystemInterface::GetInstance()->GetProcessCmdlineString(now, pid, cmdlineStr)) {
        if (!cmdlineStr.cmdline.empty()) {
            info.cmdline = cmdlineStr.cmdline[0]; // 原始的 \0 分隔的字符串

            // 早期退出：如果 cmdline 为空，不解析参数
            if (!info.cmdline.empty()) {
                // 直接遍历字符串解析参数（比 istringstream 更高效）
                const std::string& rawCmdline = info.cmdline;
                size_t start = 0;
                for (size_t i = 0; i < rawCmdline.size(); ++i) {
                    if (rawCmdline[i] == '\0') {
                        if (i > start) {
                            info.args.push_back(rawCmdline.substr(start, i - start));
                        }
                        start = i + 1;
                    }
                }
                // 处理最后一个参数（如果字符串不以 '\0' 结尾）
                if (start < rawCmdline.size()) {
                    info.args.push_back(rawCmdline.substr(start));
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

bool ProcessEntityCollector::UpdateVariableAttributes(pid_t pid, time_t now, ProcessEntityInfo& info) {
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

bool ProcessEntityCollector::ShouldCollectProcess(pid_t /* pid */, const ProcessEntityInfo& info) {
    // 过滤内核线程
    if (mFilterConfig.excludeKernelThreads) {
        // 检查 PF_KTHREAD 标志位（0x00200000）
        // 内核线程在进程创建时就设置了该标志位，这是最准确的判断方法
        if (ProcParser::IsKernelThread(info.flags)) {
            return false;
        }
    }

    // 最小运行时间过滤（0 表示不过滤）
    if (mFilterConfig.minRunningTimeSeconds > 0) {
        time_t now = time(nullptr);
        SystemInformation systemInfo;
        if (SystemInterface::GetInstance()->GetSystemInformation(systemInfo)) {
            // 计算进程运行时间
            int64_t startTimeSec = info.startTime / SYSTEM_HERTZ + systemInfo.bootTime;
            int64_t runningTime = now - startTimeSec;
            if (runningTime < mFilterConfig.minRunningTimeSeconds) {
                return false;
            }
        }
    }

    // 黑名单过滤（快速路径 + 正则表达式）
    // 1. 快速路径：简单字符串匹配
    for (const auto& pattern : mFilterConfig.simpleBlacklistPatterns) {
        if (info.comm.find(pattern) != std::string::npos || info.exe.find(pattern) != std::string::npos) {
            return false;
        }
    }
    // 2. 慢路径：正则表达式匹配
    for (const auto& re : mFilterConfig.compiledBlacklistRegexes) {
        if (std::regex_search(info.comm, re) || std::regex_search(info.exe, re)) {
            return false;
        }
    }

    // 白名单过滤（如果配置了白名单，则只采集匹配的）
    if (!mFilterConfig.simpleWhitelistPatterns.empty() || !mFilterConfig.compiledWhitelistRegexes.empty()) {
        bool matched = false;

        // 1. 快速路径：简单字符串匹配
        if (!matched) {
            for (const auto& pattern : mFilterConfig.simpleWhitelistPatterns) {
                if (info.comm.find(pattern) != std::string::npos || info.exe.find(pattern) != std::string::npos) {
                    matched = true;
                    break;
                }
            }
        }

        // 2. 慢路径：正则表达式匹配（仅在快速路径未匹配时执行）
        if (!matched) {
            for (const auto& re : mFilterConfig.compiledWhitelistRegexes) {
                if (std::regex_search(info.comm, re) || std::regex_search(info.exe, re)) {
                    matched = true;
                    break;
                }
            }
        }

        if (!matched) {
            return false;
        }
    }

    return true;
}

void ProcessEntityCollector::GenerateProcessEntityEvent(PipelineEventGroup* group,
                                                        const ProcessEntityInfo& info,
                                                        bool /* isFullReport */) {
    auto* event = group->AddLogEvent();
    // 获取包含纳秒的时间戳
    LogtailTime currentTime = GetCurrentLogtailTime();
    event->SetTimestamp(currentTime.tv_sec, static_cast<uint32_t>(currentTime.tv_nsec));

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
        event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_STATE, std::string(1, info.state));
    }

    // 运行时间（秒）- 实时计算
    int64_t runningTimeSeconds = currentTime.tv_sec - info.startTimeUnix;
    if (runningTimeSeconds > 0) {
        event->SetContent(DEFAULT_CONTENT_KEY_PROCESS_RUNTIME_SECONDS, std::to_string(runningTimeSeconds));
    }
}

void ProcessEntityCollector::AddHostLabels(PipelineEventGroup& group) {
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

void ProcessEntityCollector::InitMetrics() {
    MetricLabels labels;
    labels.emplace_back(METRIC_LABEL_KEY_RUNNER_NAME, "process_entity_collector");
    WriteMetrics::GetInstance()->CreateMetricsRecordRef(
        sMetricsRecordRef, MetricCategory::METRIC_CATEGORY_RUNNER, std::move(labels));

    sFullReportTotal = sMetricsRecordRef.CreateCounter(METRIC_RUNNER_FULL_REPORT_TOTAL);
    sIncrementalReportTotal = sMetricsRecordRef.CreateCounter(METRIC_RUNNER_INCREMENTAL_REPORT_TOTAL);
    sProcessAddedTotal = sMetricsRecordRef.CreateCounter(METRIC_RUNNER_PROCESS_ADDED_TOTAL);
    sProcessRemovedTotal = sMetricsRecordRef.CreateCounter(METRIC_RUNNER_PROCESS_REMOVED_TOTAL);
    sProcessReusedTotal = sMetricsRecordRef.CreateCounter(METRIC_RUNNER_PROCESS_REUSED_TOTAL);
    sCachedProcessCount = sMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_CACHED_PROCESS_COUNT);
    sCollectLatencyMs = sMetricsRecordRef.CreateTimeCounter(METRIC_RUNNER_COLLECT_LATENCY_MS);
    sFullCollectLatencyMs = sMetricsRecordRef.CreateTimeCounter(METRIC_RUNNER_FULL_COLLECT_LATENCY_MS);
    sIncrementalCollectLatencyMs = sMetricsRecordRef.CreateTimeCounter(METRIC_RUNNER_INCREMENTAL_COLLECT_LATENCY_MS);

    WriteMetrics::GetInstance()->CommitMetricsRecordRef(sMetricsRecordRef);

    LOG_INFO(sLogger, ("ProcessEntityCollector metrics initialized", ""));
}

} // namespace logtail
