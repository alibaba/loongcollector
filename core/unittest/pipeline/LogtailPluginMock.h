/*
 * Copyright 2024 iLogtail Authors
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

#include "go_pipeline/LogtailPlugin.h"
#ifdef APSARA_UNIT_TEST_MAIN
#include "unittest/pipeline/LogtailPluginMock.h"
#endif

namespace logtail {
class LogtailPluginMock : public LogtailPlugin {
public:
    static LogtailPluginMock* GetInstance() {
        static LogtailPluginMock instance;
        return &instance;
    }

    void BlockStart() { startBlockFlag = true; }
    void UnblockStart() { startBlockFlag = false; }
    void BlockProcess() { processBlockFlag = true; }
    void UnblockProcess() { processBlockFlag = false; }
    void BlockStop() { stopBlockFlag = true; }
    void UnblockStop() { stopBlockFlag = false; }

    void Start(const std::string& configName) {
        while (startBlockFlag) {
            LOG_DEBUG(sLogger, ("LogtailPluginMock start", "block")("config", configName));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        startFlag = true;
        LOG_INFO(sLogger, ("LogtailPluginMock start", "success")("config", configName));
    }

    void Stop(const std::string& configName, bool removingFlag) {
        while (stopBlockFlag) {
            LOG_DEBUG(sLogger, ("LogtailPluginMock stop", "block")("config", configName));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        startFlag = false;
        LOG_INFO(sLogger, ("LogtailPluginMock stop", "success")("config", configName));
    }


    void ProcessLogGroup(const std::string& configName, const std::string& logGroup, const std::string& packId) {
#ifndef APSARA_UNIT_TEST_MAIN
        while (processBlockFlag) {
            LOG_DEBUG(sLogger, ("LogtailPluginMock process log group", "block")("config", configName));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        LogtailPlugin::SendPbV2(configName.c_str(),
                                configName.size(),
                                "",
                                0,
                                const_cast<char*>(logGroup.c_str()),
                                logGroup.size(),
                                0,
                                "",
                                0);
        LOG_INFO(sLogger,
                 ("LogtailPluginMock process log group", "success")("config", configName)("logGroup",
                                                                                          logGroup)("packId", packId));
#else
        LogtailPluginMock::GetInstance()->ProcessLogGroup(configName, logGroup, packId);
#endif
    }

    bool IsStarted() const { return startFlag; }

private:
    std::atomic_bool startBlockFlag = false;
    std::atomic_bool processBlockFlag = false;
    std::atomic_bool stopBlockFlag = false;
    std::atomic_bool startFlag = false;
};

} // namespace logtail