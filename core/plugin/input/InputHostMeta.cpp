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
#include "host_monitor/entity/ProcessEntityRunner.h"
#include "logger/Logger.h"

namespace logtail {

const std::string InputHostMeta::sName = "input_host_meta";
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

    // 读取白名单
    if (config.isMember("WhitelistPatterns") && config["WhitelistPatterns"].isArray()) {
        for (const auto& pattern : config["WhitelistPatterns"]) {
            if (pattern.isString()) {
                mWhitelistPatterns.push_back(pattern.asString());
            }
        }
    }

    // 读取黑名单
    if (config.isMember("BlacklistPatterns") && config["BlacklistPatterns"].isArray()) {
        for (const auto& pattern : config["BlacklistPatterns"]) {
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
                  "skip ProcessEntityRunner initialization")("config", mContext->GetConfigName()));
        return true;
    }

    // 初始化 ProcessEntityRunner
    ProcessEntityRunner::GetInstance()->Init();

    // 注册配置到 ProcessEntityRunner
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

    ProcessEntityRunner::GetInstance()->RegisterConfig(mContext->GetConfigName(),
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
                 ("process entity collection disabled", "skip ProcessEntityRunner cleanup")("config",
                                                                                            mContext->GetConfigName()));
        return true;
    }

    // 从 ProcessEntityRunner 移除配置
    ProcessEntityRunner::GetInstance()->RemoveConfig(mContext->GetConfigName());

    return true;
}

} // namespace logtail
