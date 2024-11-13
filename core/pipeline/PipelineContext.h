/*
 * Copyright 2023 iLogtail Authors
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

#include <json/json.h>

#include <cstdint>
#include <string>

#include "logger/Logger.h"
#include "models/PipelineEventGroup.h"
#include "monitor/LogFileProfiler.h"
#include "monitor/AlarmManager.h"
#include "pipeline/GlobalConfig.h"
#include "pipeline/queue/QueueKey.h"

namespace logtail {

class Pipeline;
class FlusherSLS;

// for compatiblity with shennong profile
struct ProcessProfile {
    int readBytes = 0;
    int skipBytes = 0;
    int feedLines = 0;
    int splitLines = 0;
    int parseFailures = 0;
    int regexMatchFailures = 0;
    int parseTimeFailures = 0;
    int historyFailures = 0;
    int logGroupSize = 0;

    void Reset() { memset(this, 0, sizeof(ProcessProfile)); }
};

class PipelineContext {
public:
    PipelineContext() {}
    PipelineContext(const PipelineContext&) = delete;
    PipelineContext(PipelineContext&&) = delete;
    PipelineContext operator=(const PipelineContext&) = delete;
    PipelineContext operator=(PipelineContext&&) = delete;

    const std::string& GetConfigName() const { return mConfigName; }
    void SetConfigName(const std::string& configName) { mConfigName = configName; }
    uint32_t GetCreateTime() const { return mCreateTime; }
    void SetCreateTime(uint32_t time) { mCreateTime = time; }
    const GlobalConfig& GetGlobalConfig() const { return mGlobalConfig; }
    bool InitGlobalConfig(const Json::Value& config, Json::Value& extendedParams) {
        return mGlobalConfig.Init(config, *this, extendedParams);
    }
    void SetProcessQueueKey(QueueKey key) { mProcessQueueKey = key; }
    QueueKey GetProcessQueueKey() const { return mProcessQueueKey; }
    const Pipeline& GetPipeline() const { return *mPipeline; }
    Pipeline& GetPipeline() { return *mPipeline; }
    void SetPipeline(Pipeline& pipeline) { mPipeline = &pipeline; }

    const std::string& GetProjectName() const;
    const std::string& GetLogstoreName() const;
    const std::string& GetRegion() const;
    QueueKey GetLogstoreKey() const;
    const FlusherSLS* GetSLSInfo() const { return mSLSInfo; }
    void SetSLSInfo(const FlusherSLS* flusherSLS) { mSLSInfo = flusherSLS; }
    bool RequiringJsonReader() const { return mRequiringJsonReader; }
    void SetRequiringJsonReaderFlag(bool flag) { mRequiringJsonReader = flag; }
    bool IsFirstProcessorApsara() const { return mIsFirstProcessorApsara; }
    void SetIsFirstProcessorApsaraFlag(bool flag) { mIsFirstProcessorApsara = flag; }
    bool IsFirstProcessorJson() const { return mIsFirstProcessorJson; }
    void SetIsFirstProcessorJsonFlag(bool flag) { mIsFirstProcessorJson = flag; }
    bool IsExactlyOnceEnabled() const {return mEnableExactlyOnce; }
    void SetExactlyOnceFlag(bool flag) { mEnableExactlyOnce = flag; }

    ProcessProfile& GetProcessProfile() const { return mProcessProfile; }
    // LogFileProfiler& GetProfiler() { return *mProfiler; }
    const Logger::logger& GetLogger() const { return mLogger; }
    AlarmManager& GetAlarm() const { return *mAlarm; };

private:
    static const std::string sEmptyString;

    std::string mConfigName;
    uint32_t mCreateTime;
    GlobalConfig mGlobalConfig;
    QueueKey mProcessQueueKey = -1;
    Pipeline* mPipeline = nullptr;

    // special members for compatability
    const FlusherSLS* mSLSInfo = nullptr;
    bool mRequiringJsonReader = false;
    bool mIsFirstProcessorApsara = false;
    bool mIsFirstProcessorJson = false;
    bool mEnableExactlyOnce = false;

    mutable ProcessProfile mProcessProfile;
    // LogFileProfiler* mProfiler = LogFileProfiler::GetInstance();
    Logger::logger mLogger = sLogger;
    AlarmManager* mAlarm = AlarmManager::GetInstance();
};

} // namespace logtail
