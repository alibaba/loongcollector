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
#include <string>
#include <unordered_map>

#include "collection_pipeline/queue/QueueKey.h"
#include "host_monitor/collector/ProcessEntityCollector.h"
#include "runner/InputRunner.h"

namespace logtail {

// ProcessEntityConfigManager 是一个轻量级的配置管理器
// 负责管理 ProcessEntity 的配置，并复用 HostMonitorInputRunner 的调度逻辑
class ProcessEntityConfigManager : public InputRunner {
public:
    ProcessEntityConfigManager(const ProcessEntityConfigManager&) = delete;
    ProcessEntityConfigManager(ProcessEntityConfigManager&&) = delete;
    ProcessEntityConfigManager& operator=(const ProcessEntityConfigManager&) = delete;
    ProcessEntityConfigManager& operator=(ProcessEntityConfigManager&&) = delete;

    static ProcessEntityConfigManager* GetInstance() {
        static ProcessEntityConfigManager sInstance;
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

    // 获取配置（供 ProcessEntityCollector 查询）
    bool GetFilterConfig(const std::string& configName, ProcessFilterConfig& config) const;
    bool GetFullReportInterval(const std::string& configName, std::chrono::seconds& interval) const;

private:
    ProcessEntityConfigManager();
    ~ProcessEntityConfigManager() override = default;

    // 配置存储
    struct ProcessEntityConfig {
        ProcessFilterConfig filterConfig;
        std::chrono::seconds fullReportInterval{3600};
    };

    mutable std::mutex mConfigMutex;
    std::unordered_map<std::string, ProcessEntityConfig> mConfigs;

    // 默认间隔
    std::chrono::seconds mDefaultFullReportInterval{3600};
    std::chrono::seconds mDefaultIncrementalInterval{10};

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessEntityConfigManagerUnittest;
#endif
};

} // namespace logtail
