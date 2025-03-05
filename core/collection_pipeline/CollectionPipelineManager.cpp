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

#include "collection_pipeline/CollectionPipelineManager.h"

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "file_server/FileServer.h"
#include "go_pipeline/LogtailPlugin.h"
#include "host_monitor/HostMonitorInputRunner.h"
#include "prometheus/PrometheusInputRunner.h"
#if defined(__linux__) && !defined(__ANDROID__)
#include "ebpf/eBPFServer.h"
#endif
#include "config/feedbacker/ConfigFeedbackReceiver.h"
#include "runner/ProcessorRunner.h"
#if defined(__ENTERPRISE__) && defined(__linux__) && !defined(__ANDROID__)
#include "app_config/AppConfig.h"
#include "shennong/ShennongManager.h"
#endif

using namespace std;

namespace logtail {

CollectionPipelineManager::CollectionPipelineManager()
    : mInputRunners({
          PrometheusInputRunner::GetInstance(),
#if defined(__linux__) && !defined(__ANDROID__)
          ebpf::eBPFServer::GetInstance(),
          HostMonitorInputRunner::GetInstance(),
#endif
      }) {
}

static shared_ptr<CollectionPipeline> sEmptyPipeline;

void logtail::CollectionPipelineManager::UpdatePipelines(CollectionConfigDiff& diff) {
    // 过渡使用
    static bool isFileServerStarted = false;
    bool isFileServerInputChanged = CheckIfFileServerUpdated(diff);

#ifndef APSARA_UNIT_TEST_MAIN
#if defined(__ENTERPRISE__) && defined(__linux__) && !defined(__ANDROID__)
    if (AppConfig::GetInstance()->ShennongSocketEnabled()) {
        ShennongManager::GetInstance()->Pause();
    }
#endif
#endif
    if (isFileServerStarted && isFileServerInputChanged) {
        FileServer::GetInstance()->Pause();
    }
    // other threads only read mPipelineNameEntityMap, so we don't need to lock read here
    for (const auto& name : diff.mRemoved) {
        auto iter = mPipelineNameEntityMap.find(name);
        iter->second->Stop(true);
        DecreasePluginUsageCnt(iter->second->GetPluginStatistics());
        iter->second->RemoveProcessQueue();
        {
            unique_lock<shared_mutex> lock(mPipelineNameEntityMapMutex);
            mPipelineNameEntityMap.erase(name);
        }
        ConfigFeedbackReceiver::GetInstance().FeedbackContinuousPipelineConfigStatus(name,
                                                                                     ConfigFeedbackStatus::DELETED);
    }
    for (auto& config : diff.mModified) {
        auto p = BuildPipeline(std::move(config)); // auto reuse old pipeline's process queue and sender queue
        if (!p) {
            LOG_WARNING(sLogger,
                        ("failed to build pipeline for existing config",
                         "keep current pipeline running")("config", config.mName));
            AlarmManager::GetInstance()->SendAlarm(
                CATEGORY_CONFIG_ALARM,
                "failed to build pipeline for existing config: keep current pipeline running, config: " + config.mName,
                config.mRegion,
                config.mProject,
                config.mName,
                config.mLogstore);
            ConfigFeedbackReceiver::GetInstance().FeedbackContinuousPipelineConfigStatus(config.mName,
                                                                                         ConfigFeedbackStatus::FAILED);
            continue;
        }
        LOG_INFO(sLogger,
                 ("pipeline building for existing config succeeded",
                  "stop the old pipeline and start the new one")("config", config.mName));
        auto iter = mPipelineNameEntityMap.find(config.mName);
        iter->second->Stop(false);
        DecreasePluginUsageCnt(iter->second->GetPluginStatistics());
        {
            unique_lock<shared_mutex> lock(mPipelineNameEntityMapMutex);
            mPipelineNameEntityMap[config.mName] = p;
        }
        IncreasePluginUsageCnt(p->GetPluginStatistics());
        p->Start();
        ConfigFeedbackReceiver::GetInstance().FeedbackContinuousPipelineConfigStatus(config.mName,
                                                                                     ConfigFeedbackStatus::APPLIED);
    }
    for (auto& config : diff.mAdded) {
        auto p = BuildPipeline(std::move(config));
        if (!p) {
            LOG_WARNING(sLogger,
                        ("failed to build pipeline for new config", "skip current object")("config", config.mName));
            AlarmManager::GetInstance()->SendAlarm(
                CATEGORY_CONFIG_ALARM,
                "failed to build pipeline for new config: skip current object, config: " + config.mName,
                config.mRegion,
                config.mProject,
                config.mName,
                config.mLogstore);
            ConfigFeedbackReceiver::GetInstance().FeedbackContinuousPipelineConfigStatus(config.mName,
                                                                                         ConfigFeedbackStatus::FAILED);
            continue;
        }
        LOG_INFO(sLogger,
                 ("pipeline building for new config succeeded", "begin to start pipeline")("config", config.mName));
        {
            unique_lock<shared_mutex> lock(mPipelineNameEntityMapMutex);
            mPipelineNameEntityMap[config.mName] = p;
        }
        IncreasePluginUsageCnt(p->GetPluginStatistics());
        p->Start();
        ConfigFeedbackReceiver::GetInstance().FeedbackContinuousPipelineConfigStatus(config.mName,
                                                                                     ConfigFeedbackStatus::APPLIED);
    }

    // 在Flusher改造完成前，先不执行如下步骤，不会造成太大影响
    // Sender::CleanUnusedAk();

    if (isFileServerInputChanged) {
        if (isFileServerStarted) {
            FileServer::GetInstance()->Resume();
        } else {
            FileServer::GetInstance()->Start();
            isFileServerStarted = true;
        }
    }

#ifndef APSARA_UNIT_TEST_MAIN
#if defined(__ENTERPRISE__) && defined(__linux__) && !defined(__ANDROID__)
    if (AppConfig::GetInstance()->ShennongSocketEnabled()) {
        ShennongManager::GetInstance()->Resume();
    }
#endif
#endif

    for (auto& item : mInputRunners) {
        if (!item->HasRegisteredPlugins()) {
            item->Stop();
        }
    }
}

const shared_ptr<CollectionPipeline>& CollectionPipelineManager::FindConfigByName(const string& configName) const {
    shared_lock<shared_mutex> lock(mPipelineNameEntityMapMutex);
    auto it = mPipelineNameEntityMap.find(configName);
    if (it != mPipelineNameEntityMap.end()) {
        return it->second;
    }
    return sEmptyPipeline;
}

vector<string> CollectionPipelineManager::GetAllConfigNames() const {
    shared_lock<shared_mutex> lock(mPipelineNameEntityMapMutex);
    vector<string> res;
    for (const auto& item : mPipelineNameEntityMap) {
        res.push_back(item.first);
    }
    return res;
}

string CollectionPipelineManager::GetPluginStatistics() const {
    Json::Value root;
    ScopedSpinLock lock(mPluginCntMapLock);
    for (const auto& item : mPluginCntMap) {
        for (const auto& plugin : item.second) {
            root[item.first][plugin.first] = Json::Value(plugin.second);
        }
    }
    return root.toStyledString();
}

void CollectionPipelineManager::StopAllPipelines() {
    LOG_INFO(sLogger, ("stop all pipelines", "starts"));
    for (auto& item : mInputRunners) {
        item->Stop();
    }
    FileServer::GetInstance()->Stop();

    LogtailPlugin::GetInstance()->StopAllPipelines(true);

    Timer::GetInstance()->Stop();
    ProcessorRunner::GetInstance()->Stop();

    FlushAllBatch();

    LogtailPlugin::GetInstance()->StopAllPipelines(false);

    // Sender should be stopped after profiling threads are stopped.
    LOG_INFO(sLogger, ("stop all pipelines", "succeeded"));
}

void CollectionPipelineManager::ClearAllPipelines() {
    unique_lock<shared_mutex> lock(mPipelineNameEntityMapMutex);
    mPipelineNameEntityMap.clear();
}

shared_ptr<CollectionPipeline> CollectionPipelineManager::BuildPipeline(CollectionConfig&& config) {
    shared_ptr<CollectionPipeline> p = make_shared<CollectionPipeline>();
    // only config.mDetail is removed, other members can be safely used later
    if (!p->Init(std::move(config))) {
        return nullptr;
    }
    return p;
}

void CollectionPipelineManager::FlushAllBatch() {
    shared_lock<shared_mutex> lock(mPipelineNameEntityMapMutex);
    for (const auto& item : mPipelineNameEntityMap) {
        item.second->FlushBatch();
    }
}

void CollectionPipelineManager::IncreasePluginUsageCnt(
    const unordered_map<string, unordered_map<string, uint32_t>>& statistics) {
    for (const auto& item : statistics) {
        for (const auto& plugin : item.second) {
            mPluginCntMap[item.first][plugin.first] += plugin.second;
        }
    }
}

void CollectionPipelineManager::DecreasePluginUsageCnt(
    const unordered_map<string, unordered_map<string, uint32_t>>& statistics) {
    for (const auto& item : statistics) {
        for (const auto& plugin : item.second) {
            mPluginCntMap[item.first][plugin.first] -= plugin.second;
        }
    }
}

bool CollectionPipelineManager::CheckIfFileServerUpdated(CollectionConfigDiff& diff) {
    // private method, no need to lock mPipelineNameEntityMapMutex
    for (const auto& name : diff.mRemoved) {
        string inputType = mPipelineNameEntityMap[name]->GetConfig()["inputs"][0]["Type"].asString();
        if (inputType == "input_file" || inputType == "input_container_stdio") {
            return true;
        }
    }
    for (const auto& config : diff.mModified) {
        string inputType = (*config.mInputs[0])["Type"].asString();
        if (inputType == "input_file" || inputType == "input_container_stdio") {
            return true;
        }
    }
    for (const auto& config : diff.mAdded) {
        string inputType = (*config.mInputs[0])["Type"].asString();
        if (inputType == "input_file" || inputType == "input_container_stdio") {
            return true;
        }
    }
    return false;
}

} // namespace logtail
