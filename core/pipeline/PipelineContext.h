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
#include "models/PipelineEventGroup.h"
#include "logger/Logger.h"
#include "monitor/LogtailAlarm.h"
#include "monitor/LogFileProfiler.h"

namespace logtail {

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

    const std::string& GetProjectName() const { return mProjectName; }
    void SetProjectName(const std::string& projectName) { mProjectName = projectName; }
    const std::string& GetLogstoreName() const { return mLogstoreName; }
    void SetLogstoreName(const std::string& logstoreName) { mLogstoreName = logstoreName; }
    const std::string& GetConfigName() const { return mConfigName; }
    void SetConfigName(const std::string& configName) { mConfigName = configName; }
    const std::string& GetRegion() const { return mRegion; }
    void SetRegion(const std::string& region) { mRegion = region; }

    ProcessProfile& GetProcessProfile() { return mProcessProfile; }
    // LogFileProfiler& GetProfiler() { return *mProfiler; }
    Logger::logger& GetLogger() { return mLogger; }
    LogtailAlarm& GetAlarm() { return *mAlarm; };

private:
    std::string mProjectName, mLogstoreName, mConfigName, mRegion;
    ProcessProfile mProcessProfile;
    // LogFileProfiler* mProfiler = LogFileProfiler::GetInstance();
    Logger::logger mLogger = sLogger;
    LogtailAlarm* mAlarm = LogtailAlarm::GetInstance();
};
} // namespace logtail