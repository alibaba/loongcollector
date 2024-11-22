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

#include "pipeline/PipelineManager.h"

#include "file_server/ConfigManager.h"
#include "file_server/FileServer.h"
#include "go_pipeline/LogtailPlugin.h"
#include "prometheus/PrometheusInputRunner.h"
#if defined(__linux__) && !defined(__ANDROID__)
#include "ebpf/eBPFServer.h"
#endif
#include "runner/ProcessorRunner.h"
#if defined(__ENTERPRISE__) && defined(__linux__) && !defined(__ANDROID__)
#include "app_config/AppConfig.h"
#include "shennong/ShennongManager.h"
#endif
#include "config/feedbacker/ConfigFeedbackReceiver.h"
#include "pipeline/queue/ProcessQueueManager.h"
#include "pipeline/queue/QueueKeyManager.h"

using namespace std;

namespace logtail {

PipelineManager::PipelineManager()
    : mInputRunners({
          PrometheusInputRunner::GetInstance(),
#if defined(__linux__) && !defined(__ANDROID__)
              ebpf::eBPFServer::GetInstance(),
#endif
      }) {
}

static shared_ptr<Pipeline> sEmptyPipeline;

void logtail::PipelineManager::UpdatePipelines(PipelineConfigDiff& diff) {
#ifndef APSARA_UNIT_TEST_MAIN
    // 过渡使用
    static bool isFileServerStarted = false;
    bool isFileServerInputChanged = false;
    for (const auto& name : diff.mRemoved) {
        isFileServerInputChanged = CheckIfFileServerUpdated(mPipelineNameEntityMap[name]->GetConfig()["inputs"][0]);
    }
    for (const auto& config : diff.mModified) {
        isFileServerInputChanged = CheckIfFileServerUpdated(*config.mInputs[0]);
    }
    for (const auto& config : diff.mAdded) {
        isFileServerInputChanged = CheckIfFileServerUpdated(*config.mInputs[0]);
    }

#if defined(__ENTERPRISE__) && defined(__linux__) && !defined(__ANDROID__)
    if (AppConfig::GetInstance()->ShennongSocketEnabled()) {
        ShennongManager::GetInstance()->Pause();
    }
#endif
    if (isFileServerStarted && isFileServerInputChanged) {
        FileServer::GetInstance()->Pause();
    }
#endif

    for (const auto& name : diff.mRemoved) {
        auto iter = mPipelineNameEntityMap.find(name);
        iter->second->Stop(true);
        DecreasePluginUsageCnt(iter->second->GetPluginStatistics());
        iter->second->RemoveProcessQueue();
        mPipelineNameEntityMap.erase(iter);
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
                config.mProject,
                config.mLogstore,
                config.mRegion);
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

        mPipelineNameEntityMap[config.mName] = p;
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
                config.mProject,
                config.mLogstore,
                config.mRegion);
            ConfigFeedbackReceiver::GetInstance().FeedbackContinuousPipelineConfigStatus(config.mName,
                                                                                         ConfigFeedbackStatus::FAILED);
            continue;
        }
        LOG_INFO(sLogger,
                 ("pipeline building for new config succeeded", "begin to start pipeline")("config", config.mName));
        mPipelineNameEntityMap[config.mName] = p;
        IncreasePluginUsageCnt(p->GetPluginStatistics());
        p->Start();
        ConfigFeedbackReceiver::GetInstance().FeedbackContinuousPipelineConfigStatus(config.mName,
                                                                                     ConfigFeedbackStatus::APPLIED);
    }

#ifndef APSARA_UNIT_TEST_MAIN
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

const shared_ptr<Pipeline>& PipelineManager::FindConfigByName(const string& configName) const {
    auto it = mPipelineNameEntityMap.find(configName);
    if (it != mPipelineNameEntityMap.end()) {
        return it->second;
    }
    return sEmptyPipeline;
}

vector<string> PipelineManager::GetAllConfigNames() const {
    vector<string> res;
    for (const auto& item : mPipelineNameEntityMap) {
        res.push_back(item.first);
    }
    return res;
}

string PipelineManager::GetPluginStatistics() const {
    Json::Value root;
    ScopedSpinLock lock(mPluginCntMapLock);
    for (const auto& item : mPluginCntMap) {
        for (const auto& plugin : item.second) {
            root[item.first][plugin.first] = Json::Value(plugin.second);
        }
    }
    return root.toStyledString();
}

void PipelineManager::StopAllPipelines() {
    LOG_INFO(sLogger, ("stop all pipelines", "starts"));
    for (auto& item : mInputRunners) {
        item->Stop();
    }
    FileServer::GetInstance()->Stop();

    LogtailPlugin::GetInstance()->StopAllPipelines(true);

    ProcessorRunner::GetInstance()->Stop();

    FlushAllBatch();

    LogtailPlugin::GetInstance()->StopAllPipelines(false);

    // Sender should be stopped after profiling threads are stopped.
    LOG_INFO(sLogger, ("stop all pipelines", "succeeded"));
}

shared_ptr<Pipeline> PipelineManager::BuildPipeline(PipelineConfig&& config) {
    shared_ptr<Pipeline> p = make_shared<Pipeline>();
    // only config.mDetail is removed, other members can be safely used later
    if (!p->Init(std::move(config))) {
        return nullptr;
    }
    return p;
}

void PipelineManager::FlushAllBatch() {
    for (const auto& item : mPipelineNameEntityMap) {
        item.second->FlushBatch();
    }
}

void PipelineManager::IncreasePluginUsageCnt(const unordered_map<string, unordered_map<string, uint32_t>>& statistics) {
    for (const auto& item : statistics) {
        for (const auto& plugin : item.second) {
            mPluginCntMap[item.first][plugin.first] += plugin.second;
        }
    }
}

void PipelineManager::DecreasePluginUsageCnt(const unordered_map<string, unordered_map<string, uint32_t>>& statistics) {
    for (const auto& item : statistics) {
        for (const auto& plugin : item.second) {
            mPluginCntMap[item.first][plugin.first] -= plugin.second;
        }
    }
}

bool PipelineManager::CheckIfFileServerUpdated(const Json::Value& config) {
    string inputType = config["Type"].asString();
    return inputType == "input_file" || inputType == "input_container_stdio";
}

} // namespace logtail
