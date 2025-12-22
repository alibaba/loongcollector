// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "plugin/input/InputHostMeta.h"

#include <cstdint>

#include "json/value.h"

#include "common/ParamExtractor.h"
#include "constants/EntityConstants.h"
#include "host_monitor/collector/CollectorConstants.h"
#include "host_monitor/entity/ProcessEntityConfigManager.h"
#include "logger/Logger.h"

namespace logtail {

const std::string InputHostMeta::sName = std::string(kInputHostMeta);
const uint32_t kMinInterval = 5; // seconds
const uint32_t kDefaultInterval = 15; // seconds

bool InputHostMeta::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    std::string errorMsg;
    mInterval = kDefaultInterval;
    if (!GetOptionalUIntParam(config, "Interval", mInterval, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mInterval,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }
    if (mInterval < kMinInterval) {
        LOG_WARNING(sLogger,
                    ("input host meta", "interval is too small, set to min interval")("original interval", mInterval)(
                        "new interval", kMinInterval));
        mInterval = kMinInterval;
    }

    // 读取 EnableProcessEntity 配置（默认 true）
    mEnableProcessEntity = true;
    if (!GetOptionalBoolParam(config, "EnableProcessEntity", mEnableProcessEntity, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mEnableProcessEntity,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    LOG_INFO(sLogger,
             ("input_host_meta init", "")("enable_process_entity", mEnableProcessEntity)("config",
                                                                                         mContext->GetConfigName()));

    // 如果未启用进程实体采集，直接返回
    if (!mEnableProcessEntity) {
        LOG_INFO(sLogger,
                 ("process entity collection disabled",
                  "skip reading process entity config")("config", mContext->GetConfigName()));
        return true;
    }

    // 读取 FullReportInterval 配置（默认3600秒）
    mFullReportInterval = 3600;
    if (!GetOptionalUIntParam(config, "FullReportInterval", mFullReportInterval, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mFullReportInterval,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    // 读取 IncrementalInterval 配置（默认10秒）
    mIncrementalInterval = 10;
    if (!GetOptionalUIntParam(config, "IncrementalInterval", mIncrementalInterval, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mIncrementalInterval,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    // 确保 IncrementalInterval 不小于最小值
    if (mIncrementalInterval < kMinInterval) {
        LOG_WARNING(sLogger,
                    ("input host meta", "incremental interval is too small, set to min interval")(
                        "original interval", mIncrementalInterval)("new interval", kMinInterval));
        mIncrementalInterval = kMinInterval;
    }

    // 读取过滤配置（过滤逻辑始终应用）
    if (!GetOptionalBoolParam(config, "ExcludeKernelThreads", mExcludeKernelThreads, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mExcludeKernelThreads,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    if (!GetOptionalUIntParam(config, "MinRunningTimeSeconds", mMinRunningTimeSeconds, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mMinRunningTimeSeconds,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    // 读取白名单（支持新旧两种配置参数名）
    // 优先使用 ProcessWhitelist（文档中的名称），向后兼容 WhitelistPatterns
    const char* whitelistKey = nullptr;
    if (config.isMember("ProcessWhitelist") && config["ProcessWhitelist"].isArray()) {
        whitelistKey = "ProcessWhitelist";
    } else if (config.isMember("WhitelistPatterns") && config["WhitelistPatterns"].isArray()) {
        whitelistKey = "WhitelistPatterns";
    }
    if (whitelistKey != nullptr) {
        for (const auto& pattern : config[whitelistKey]) {
            if (pattern.isString()) {
                mWhitelistPatterns.push_back(pattern.asString());
            }
        }
    }

    // 读取黑名单（支持新旧两种配置参数名）
    // 优先使用 ProcessBlacklist（文档中的名称），向后兼容 BlacklistPatterns
    const char* blacklistKey = nullptr;
    if (config.isMember("ProcessBlacklist") && config["ProcessBlacklist"].isArray()) {
        blacklistKey = "ProcessBlacklist";
    } else if (config.isMember("BlacklistPatterns") && config["BlacklistPatterns"].isArray()) {
        blacklistKey = "BlacklistPatterns";
    }
    if (blacklistKey != nullptr) {
        for (const auto& pattern : config[blacklistKey]) {
            if (pattern.isString()) {
                mBlacklistPatterns.push_back(pattern.asString());
            }
        }
    }

    return true;
}

bool InputHostMeta::Start() {
    // 如果未启用进程实体采集，直接返回
    if (!mEnableProcessEntity) {
        LOG_INFO(sLogger,
                 ("process entity collection disabled",
                  "skip ProcessEntityConfigManager initialization")("config", mContext->GetConfigName()));
        return true;
    }

    // 初始化 ProcessEntityConfigManager（内部会初始化 HostMonitorInputRunner）
    ProcessEntityConfigManager::GetInstance()->Init();

    // 注册配置到 ProcessEntityConfigManager（内部会注册到 HostMonitorInputRunner）
    ProcessFilterConfig filterConfig;
    filterConfig.excludeKernelThreads = mExcludeKernelThreads;
    filterConfig.minRunningTimeSeconds = mMinRunningTimeSeconds;
    filterConfig.whitelistPatterns = mWhitelistPatterns;
    filterConfig.blacklistPatterns = mBlacklistPatterns;

    LOG_INFO(sLogger,
             ("ProcessEntity filter config", "")("exclude_kernel_threads", filterConfig.excludeKernelThreads)(
                 "min_running_time_seconds", filterConfig.minRunningTimeSeconds)("whitelist_patterns_count",
                                                                                 filterConfig.whitelistPatterns.size())(
                 "blacklist_patterns_count", filterConfig.blacklistPatterns.size()));

    // 注册配置：ProcessEntityConfigManager 会将配置转发给 HostMonitorInputRunner
    ProcessEntityConfigManager::GetInstance()->RegisterConfig(mContext->GetConfigName(),
                                                              mContext->GetProcessQueueKey(),
                                                              mIndex,
                                                              filterConfig,
                                                              mIncrementalInterval,
                                                              mFullReportInterval);
    return true;
}

bool InputHostMeta::Stop(bool isPipelineRemoving) {
    // 如果未启用进程实体采集，直接返回
    if (!mEnableProcessEntity) {
        LOG_INFO(sLogger,
                 ("process entity collection disabled",
                  "skip ProcessEntityConfigManager cleanup")("config", mContext->GetConfigName()));
        return true;
    }

    // 从 ProcessEntityConfigManager 移除配置
    ProcessEntityConfigManager::GetInstance()->RemoveConfig(mContext->GetConfigName());

    return true;
}

} // namespace logtail
