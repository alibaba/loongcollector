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

#include "host_monitor/entity/ProcessEntityConfigManager.h"

#include "host_monitor/HostMonitorInputRunner.h"
#include "host_monitor/collector/ProcessEntityCollector.h"
#include "logger/Logger.h"

namespace logtail {

// ========== ProcessEntityConfigManager 方法实现 ==========

ProcessEntityConfigManager::ProcessEntityConfigManager() {
}

void ProcessEntityConfigManager::Init() {
    // 委托给 HostMonitorInputRunner
    HostMonitorInputRunner::GetInstance()->Init();
    LOG_INFO(sLogger, ("ProcessEntityConfigManager initialized (using HostMonitorInputRunner)", ""));
}

void ProcessEntityConfigManager::Stop() {
    // 移除所有配置
    RemoveAllConfigs();
    LOG_INFO(sLogger, ("ProcessEntityConfigManager stopped", ""));
}

bool ProcessEntityConfigManager::HasRegisteredPlugins() const {
    std::lock_guard<std::mutex> lock(mConfigMutex);
    return !mConfigs.empty();
}

void ProcessEntityConfigManager::RegisterConfig(const std::string& configName,
                                                QueueKey processQueueKey,
                                                size_t inputIndex,
                                                const ProcessFilterConfig& filterConfig,
                                                uint32_t incrementalIntervalSeconds,
                                                uint32_t fullReportIntervalSeconds) {
    std::lock_guard<std::mutex> lock(mConfigMutex);

    auto it = mConfigs.find(configName);
    if (it != mConfigs.end()) {
        LOG_ERROR(sLogger,
                  ("config already registered",
                   "cannot have multiple input_host_meta instances with EnableProcessEntity=true in same pipeline")(
                      "configName", configName)("inputIndex", inputIndex)("action", "reject registration")(
                      "suggestion", "set EnableProcessEntity=false for additional instances"));
        return;
    }

    // 预编译正则表达式
    ProcessFilterConfig compiledFilterConfig = filterConfig;
    if (!compiledFilterConfig.CompileRegexes()) {
        LOG_ERROR(sLogger,
                  ("failed to compile regex patterns",
                   "process entity collection disabled for this config")("configName", configName));
        return;
    }

    // 保存配置
    ProcessEntityConfig config;
    config.filterConfig = compiledFilterConfig;
    config.fullReportInterval
        = fullReportIntervalSeconds > 0 ? std::chrono::seconds(fullReportIntervalSeconds) : mDefaultFullReportInterval;
    mConfigs[configName] = config;

    // 注册到 ProcessEntityCollector 全局配置
    ProcessEntityCollector::RegisterConfig(configName, compiledFilterConfig, config.fullReportInterval);

    // 使用 HostMonitorInputRunner 注册 collector
    auto incrementalInterval = incrementalIntervalSeconds > 0
        ? incrementalIntervalSeconds
        : static_cast<uint32_t>(mDefaultIncrementalInterval.count());

    CollectorInfo collectorInfo;
    collectorInfo.name = ProcessEntityCollector::sName;
    collectorInfo.interval = incrementalInterval;
    collectorInfo.type = HostMonitorCollectType::kSingleValue;

    std::vector<CollectorInfo> collectorInfos = {collectorInfo};
    HostMonitorInputRunner::GetInstance()->UpdateCollector(configName, collectorInfos, processQueueKey, inputIndex);

    LOG_INFO(
        sLogger,
        ("registered process entity config", configName)("exclude_kernel_threads", filterConfig.excludeKernelThreads)(
            "min_running_time_seconds", filterConfig.minRunningTimeSeconds)(
            "incremental_interval", incrementalInterval)("full_report_interval", config.fullReportInterval.count()));
}

void ProcessEntityConfigManager::RemoveConfig(const std::string& configName) {
    std::lock_guard<std::mutex> lock(mConfigMutex);
    auto it = mConfigs.find(configName);
    if (it != mConfigs.end()) {
        mConfigs.erase(it);
        LOG_INFO(sLogger, ("removed process entity config", configName));
    }

    // 从全局配置中注销
    ProcessEntityCollector::UnregisterConfig(configName);

    // 从 HostMonitorInputRunner 移除 collector
    HostMonitorInputRunner::GetInstance()->RemoveCollector(configName);
}

void ProcessEntityConfigManager::RemoveAllConfigs() {
    std::lock_guard<std::mutex> lock(mConfigMutex);

    // 注销所有全局配置
    for (const auto& [configName, config] : mConfigs) {
        ProcessEntityCollector::UnregisterConfig(configName);
        HostMonitorInputRunner::GetInstance()->RemoveCollector(configName);
    }

    mConfigs.clear();
    LOG_INFO(sLogger, ("removed all process entity configs", ""));
}

void ProcessEntityConfigManager::SetFullReportInterval(std::chrono::seconds interval) {
    mDefaultFullReportInterval = interval;
    LOG_INFO(sLogger, ("updated default full report interval", interval.count()));
}

void ProcessEntityConfigManager::UpdateFullReportInterval(const std::string& configName,
                                                          std::chrono::seconds interval) {
    // 更新本地配置
    {
        std::lock_guard<std::mutex> lock(mConfigMutex);
        auto it = mConfigs.find(configName);
        if (it != mConfigs.end()) {
            it->second.fullReportInterval = interval;
        } else {
            LOG_WARNING(sLogger, ("config not found, cannot update interval", configName));
            return;
        }
    }

    // 更新全局配置（线程安全，Collector 在下次 Collect 时会读取最新值）
    {
        std::lock_guard<std::mutex> lock(ProcessEntityCollector::sConfigMutex);
        auto it = ProcessEntityCollector::sConfigs.find(configName);
        if (it != ProcessEntityCollector::sConfigs.end()) {
            it->second.fullReportInterval = interval;
            LOG_INFO(sLogger,
                     ("updated full report interval for config", configName)("new_interval", interval.count()));
        }
    }
}

void ProcessEntityConfigManager::TriggerFullReport(const std::string& configName) {
    ProcessEntityCollector::TriggerFullReport(configName);
}

bool ProcessEntityConfigManager::GetFilterConfig(const std::string& configName, ProcessFilterConfig& config) const {
    std::lock_guard<std::mutex> lock(mConfigMutex);
    auto it = mConfigs.find(configName);
    if (it != mConfigs.end()) {
        config = it->second.filterConfig;
        return true;
    }
    return false;
}

bool ProcessEntityConfigManager::GetFullReportInterval(const std::string& configName,
                                                       std::chrono::seconds& interval) const {
    std::lock_guard<std::mutex> lock(mConfigMutex);
    auto it = mConfigs.find(configName);
    if (it != mConfigs.end()) {
        interval = it->second.fullReportInterval;
        return true;
    }
    return false;
}

} // namespace logtail
