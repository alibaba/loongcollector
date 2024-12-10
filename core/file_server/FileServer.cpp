// Copyright 2023 iLogtail Authors
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

#include "file_server/FileServer.h"

#include "checkpoint/CheckPointManager.h"
#include "common/Flags.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "config_manager/ConfigManager.h"
#include "controller/EventDispatcher.h"
#include "event_handler/LogInput.h"
#include "input/InputFile.h"
#include "polling/PollingDirFile.h"
#include "polling/PollingModify.h"

DEFINE_FLAG_BOOL(enable_polling_discovery, "", true);

using namespace std;

namespace logtail {

void FileServer::Start() {
    ConfigManager::GetInstance()->LoadDockerConfig();
    CheckPointManager::Instance()->LoadCheckPoint();
    LOG_INFO(sLogger, ("watch dirs", "start"));
    auto start = GetCurrentTimeInMilliSeconds();
    ConfigManager::GetInstance()->RegisterHandlers();
    auto costMs = GetCurrentTimeInMilliSeconds() - start;
    if (costMs >= 60 * 1000) {
        LogtailAlarm::GetInstance()->SendAlarm(REGISTER_HANDLERS_TOO_SLOW_ALARM,
                                               "Registering handlers took " + ToString(costMs) + " ms");
        LOG_WARNING(sLogger, ("watch dirs", "succeeded")("costMs", costMs));
    } else {
        LOG_INFO(sLogger, ("watch dirs", "succeeded")("costMs", costMs));
    }
    LOG_INFO(sLogger, ("watch dirs", "succeeded"));
    EventDispatcher::GetInstance()->AddExistedCheckPointFileEvents();
    // the dump time must be reset after dir registration, since it may take long on NFS.
    CheckPointManager::Instance()->ResetLastDumpTime();
    if (BOOL_FLAG(enable_polling_discovery)) {
        PollingModify::GetInstance()->Start();
        PollingDirFile::GetInstance()->Start();
    }
    LogInput::GetInstance()->Start();
    LOG_INFO(sLogger, ("file server", "started"));
}

void FileServer::Pause(bool isConfigUpdate) {
    PauseInner();
    if (isConfigUpdate) {
        EventDispatcher::GetInstance()->DumpAllHandlersMeta(true);
        CheckPointManager::Instance()->DumpCheckPointToLocal();
        EventDispatcher::GetInstance()->ClearBrokenLinkSet();
        PollingDirFile::GetInstance()->ClearCache();
        ConfigManager::GetInstance()->ClearFilePipelineMatchCache();
    }
}

void FileServer::PauseInner() {
    LOG_INFO(sLogger, ("file server pause", "starts"));
    // cache must be cleared at last, since logFileReader dump still requires the cache
    auto holdOnStart = GetCurrentTimeInMilliSeconds();
    if (BOOL_FLAG(enable_polling_discovery)) {
        PollingDirFile::GetInstance()->HoldOn();
        PollingModify::GetInstance()->HoldOn();
    }
    LogInput::GetInstance()->HoldOn();
    auto holdOnCost = GetCurrentTimeInMilliSeconds() - holdOnStart;
    if (holdOnCost >= 60 * 1000) {
        LogtailAlarm::GetInstance()->SendAlarm(HOLD_ON_TOO_SLOW_ALARM,
                                               "Pausing file server took " + ToString(holdOnCost) + "ms");
    }
    LOG_INFO(sLogger, ("file server pause", "succeeded")("cost", ToString(holdOnCost) + "ms"));
}

void FileServer::Resume(bool isConfigUpdate) {
    if (isConfigUpdate) {
        ClearContainerInfo();
        ConfigManager::GetInstance()->DoUpdateContainerPaths();
        ConfigManager::GetInstance()->SaveDockerConfig();
    }

    LOG_INFO(sLogger, ("file server resume", "starts"));
    ConfigManager::GetInstance()->RegisterHandlers();
    LOG_INFO(sLogger, ("watch dirs", "succeeded"));
    if (isConfigUpdate) {
        EventDispatcher::GetInstance()->AddExistedCheckPointFileEvents();
    }
    LogInput::GetInstance()->Resume();
    if (BOOL_FLAG(enable_polling_discovery)) {
        PollingModify::GetInstance()->Resume();
        PollingDirFile::GetInstance()->Resume();
    }
    LOG_INFO(sLogger, ("file server resume", "succeeded"));
}

void FileServer::Stop() {
    PauseInner();
    EventDispatcher::GetInstance()->DumpAllHandlersMeta(false);
    CheckPointManager::Instance()->DumpCheckPointToLocal();
}

FileDiscoveryConfig FileServer::GetFileDiscoveryConfig(const string& name) const {
    auto itr = mPipelineNameFileDiscoveryConfigsMap.find(name);
    if (itr != mPipelineNameFileDiscoveryConfigsMap.end()) {
        return itr->second;
    }
    return make_pair(nullptr, nullptr);
}

void FileServer::AddFileDiscoveryConfig(const string& name, FileDiscoveryOptions* opts, const PipelineContext* ctx) {
    mPipelineNameFileDiscoveryConfigsMap[name] = make_pair(opts, ctx);
}

void FileServer::RemoveFileDiscoveryConfig(const string& name) {
    mPipelineNameFileDiscoveryConfigsMap.erase(name);
}

FileReaderConfig FileServer::GetFileReaderConfig(const string& name) const {
    auto itr = mPipelineNameFileReaderConfigsMap.find(name);
    if (itr != mPipelineNameFileReaderConfigsMap.end()) {
        return itr->second;
    }
    return make_pair(nullptr, nullptr);
}

void FileServer::AddFileReaderConfig(const string& name, const FileReaderOptions* opts, const PipelineContext* ctx) {
    mPipelineNameFileReaderConfigsMap[name] = make_pair(opts, ctx);
}

void FileServer::RemoveFileReaderConfig(const string& name) {
    mPipelineNameFileReaderConfigsMap.erase(name);
}

MultilineConfig FileServer::GetMultilineConfig(const string& name) const {
    auto itr = mPipelineNameMultilineConfigsMap.find(name);
    if (itr != mPipelineNameMultilineConfigsMap.end()) {
        return itr->second;
    }
    return make_pair(nullptr, nullptr);
}

void FileServer::AddMultilineConfig(const string& name, const MultilineOptions* opts, const PipelineContext* ctx) {
    mPipelineNameMultilineConfigsMap[name] = make_pair(opts, ctx);
}

void FileServer::RemoveMultilineConfig(const string& name) {
    mPipelineNameMultilineConfigsMap.erase(name);
}

void FileServer::SaveContainerInfo(const string& pipeline, const shared_ptr<vector<DockerContainerPath>>& info) {
    mAllDockerContainerPathMap[pipeline] = info;
}

shared_ptr<vector<DockerContainerPath>> FileServer::GetAndRemoveContainerInfo(const string& pipeline) {
    auto iter = mAllDockerContainerPathMap.find(pipeline);
    if (iter == mAllDockerContainerPathMap.end()) {
        return make_shared<vector<DockerContainerPath>>();
    }
    auto res = iter->second;
    mAllDockerContainerPathMap.erase(iter);
    return res;
}

void FileServer::ClearContainerInfo() {
    mAllDockerContainerPathMap.clear();
}

uint32_t FileServer::GetExactlyOnceConcurrency(const string& name) const {
    auto itr = mPipelineNameEOConcurrencyMap.find(name);
    if (itr != mPipelineNameEOConcurrencyMap.end()) {
        return itr->second;
    }
    return 0;
}

vector<string> FileServer::GetExactlyOnceConfigs() const {
    vector<string> res;
    for (const auto& item : mPipelineNameEOConcurrencyMap) {
        if (item.second > 0) {
            res.push_back(item.first);
        }
    }
    return res;
}

void FileServer::AddExactlyOnceConcurrency(const string& name, uint32_t concurrency) {
    mPipelineNameEOConcurrencyMap[name] = concurrency;
}

void FileServer::RemoveExactlyOnceConcurrency(const string& name) {
    mPipelineNameEOConcurrencyMap.erase(name);
}

} // namespace logtail
