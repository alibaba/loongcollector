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

#include "config_manager/ConfigManager.h"
#include "file_server/FileServer.h"
#include "go_pipeline/LogtailPlugin.h"
#include "prometheus/PrometheusInputRunner.h"
#if defined(__linux__) && !defined(__ANDROID__)
#include "observer/ObserverManager.h"
#include "ebpf/eBPFServer.h"
#endif
#include "processor/daemon/LogProcess.h"
#if defined(__ENTERPRISE__) && defined(__linux__) && !defined(__ANDROID__)
#include "app_config/AppConfig.h"
#include "shennong/ShennongManager.h"
#include "streamlog/StreamLogManager.h"
#endif
#include "config/feedbacker/ConfigFeedbackReceiver.h"
#include "queue/ProcessQueueManager.h"
#include "queue/QueueKeyManager.h"

using namespace std;

namespace logtail {

void logtail::PipelineManager::UpdatePipelines(PipelineConfigDiff& diff) {
#ifndef APSARA_UNIT_TEST_MAIN
    // 过渡使用
    static bool isFileServerStarted = false, isInputObserverStarted = false;
#if defined(__ENTERPRISE__) && defined(__linux__) && !defined(__ANDROID__)
    static bool isInputStreamStarted = false;
#endif
    bool isInputObserverChanged = false, isInputFileChanged = false, isInputStreamChanged = false,
         isInputContainerStdioChanged = false, isInputPrometheusChanged = false, inputEbpfChanged = false;
    for (const auto& name : diff.mRemoved) {
        CheckIfInputUpdated(mPipelineNameEntityMap[name]->GetConfig()["inputs"][0],
                            isInputObserverChanged,
                            isInputFileChanged,
                            isInputStreamChanged,
                            isInputContainerStdioChanged,
                            isInputPrometheusChanged,
                            inputEbpfChanged);
    }
    for (const auto& config : diff.mModified) {
        CheckIfInputUpdated(*config.mInputs[0],
                            isInputObserverChanged,
                            isInputFileChanged,
                            isInputStreamChanged,
                            isInputContainerStdioChanged,
                            isInputPrometheusChanged,
                            inputEbpfChanged);
    }
    for (const auto& config : diff.mAdded) {
        CheckIfInputUpdated(*config.mInputs[0],
                            isInputObserverChanged,
                            isInputFileChanged,
                            isInputStreamChanged,
                            isInputContainerStdioChanged,
                            isInputPrometheusChanged,
                            inputEbpfChanged);
    }

#if defined(__ENTERPRISE__) && defined(__linux__) && !defined(__ANDROID__)
    if (AppConfig::GetInstance()->ShennongSocketEnabled()) {
        ShennongManager::GetInstance()->Pause();
    }
    if (isInputStreamStarted && isInputStreamChanged) {
        StreamLogManager::GetInstance()->ShutdownConfigUsage();
    }
#endif
#if defined(__linux__) && !defined(__ANDROID__)
    if (isInputObserverStarted && isInputObserverChanged) {
        ObserverManager::GetInstance()->HoldOn(false);
    }
#endif
    if (isFileServerStarted && (isInputFileChanged || isInputContainerStdioChanged)) {
        FileServer::GetInstance()->Pause();
    }
    LogProcess::GetInstance()->HoldOn();
    LogtailPlugin::GetInstance()->HoldOn(false);
    if (isInputPrometheusChanged) {
        PrometheusInputRunner::GetInstance()->Start();
    }
#if defined(__linux__) && !defined(__ANDROID__)
    // 和其它插件不同，ebpf需要init之后才能配置加载，最终状态这个init函数是在插件自己的start函数里面，目前暂时在此过渡。
    if (inputEbpfChanged) {
        logtail::ebpf::eBPFServer::GetInstance()->Init();
    }

#endif
#endif

    for (const auto& name : diff.mRemoved) {
        auto iter = mPipelineNameEntityMap.find(name);
        iter->second->Stop(true);
        DecreasePluginUsageCnt(iter->second->GetPluginStatistics());
        iter->second->RemoveProcessQueue();
        mPipelineNameEntityMap.erase(iter);
        ConfigFeedbackReceiver::GetInstance().FeedbackPipelineConfigStatus(name, ConfigFeedbackStatus::DELETED);
    }
    for (auto& config : diff.mModified) {
        auto p = BuildPipeline(std::move(config));
        if (!p) {
            LOG_WARNING(sLogger,
                        ("failed to build pipeline for existing config",
                         "keep current pipeline running")("config", config.mName));
            LogtailAlarm::GetInstance()->SendAlarm(
                CATEGORY_CONFIG_ALARM,
                "failed to build pipeline for existing config: keep current pipeline running, config: " + config.mName,
                config.mProject,
                config.mLogstore,
                config.mRegion);
            diff.mUnchanged.push_back(config.mName);
            ConfigFeedbackReceiver::GetInstance().FeedbackPipelineConfigStatus(config.mName,
                                                                               ConfigFeedbackStatus::FAILED);
            continue;
        }
        ConfigFeedbackReceiver::GetInstance().FeedbackPipelineConfigStatus(config.mName, ConfigFeedbackStatus::APPLIED);
        LOG_INFO(sLogger,
                 ("pipeline building for existing config succeeded",
                  "stop the old pipeline and start the new one")("config", config.mName));

        auto iter = mPipelineNameEntityMap.find(config.mName);
        iter->second->Stop(false);
        DecreasePluginUsageCnt(iter->second->GetPluginStatistics());
        mPipelineNameEntityMap[config.mName] = p;
        IncreasePluginUsageCnt(p->GetPluginStatistics());
        p->Start();
    }
    for (auto& config : diff.mAdded) {
        auto p = BuildPipeline(std::move(config));
        if (!p) {
            LOG_WARNING(sLogger,
                        ("failed to build pipeline for new config", "skip current object")("config", config.mName));
            LogtailAlarm::GetInstance()->SendAlarm(
                CATEGORY_CONFIG_ALARM,
                "failed to build pipeline for new config: skip current object, config: " + config.mName,
                config.mProject,
                config.mLogstore,
                config.mRegion);
            ConfigFeedbackReceiver::GetInstance().FeedbackPipelineConfigStatus(config.mName,
                                                                               ConfigFeedbackStatus::FAILED);
            continue;
        }
        LOG_INFO(sLogger,
                 ("pipeline building for new config succeeded", "begin to start pipeline")("config", config.mName));
        ConfigFeedbackReceiver::GetInstance().FeedbackPipelineConfigStatus(config.mName, ConfigFeedbackStatus::APPLIED);
        mPipelineNameEntityMap[config.mName] = p;
        IncreasePluginUsageCnt(p->GetPluginStatistics());
        p->Start();
    }

#ifndef APSARA_UNIT_TEST_MAIN
    // 过渡使用，有变更的流水线的Go流水线加载在BuildPipeline中完成
    for (auto& name : diff.mUnchanged) {
        mPipelineNameEntityMap[name]->LoadGoPipelines();
    }
    // 在Flusher改造完成前，先不执行如下步骤，不会造成太大影响
    // Sender::CleanUnusedAk();

    // 过渡使用
    LogtailPlugin::GetInstance()->Resume();
    LogProcess::GetInstance()->Resume();
    if (isInputFileChanged || isInputContainerStdioChanged) {
        if (isFileServerStarted) {
            FileServer::GetInstance()->Resume();
        } else {
            FileServer::GetInstance()->Start();
            isFileServerStarted = true;
        }
    }

#if defined(__linux__) && !defined(__ANDROID__)
    if (isInputObserverChanged) {
        if (isInputObserverStarted) {
            ObserverManager::GetInstance()->Resume();
        } else {
            // input_observer_network always relies on PluginBase
            LogtailPlugin::GetInstance()->LoadPluginBase();
            ObserverManager::GetInstance()->Reload();
            isInputObserverStarted = true;
        }
    }
#endif
#if defined(__ENTERPRISE__) && defined(__linux__) && !defined(__ANDROID__)
    if (isInputStreamChanged) {
        if (isInputStreamStarted) {
            StreamLogManager::GetInstance()->StartupConfigUsage();
        } else {
            if (AppConfig::GetInstance()->GetOpenStreamLog()) {
                StreamLogManager::GetInstance()->Init();
                isInputStreamStarted = true;
            }
        }
    }
    if (AppConfig::GetInstance()->ShennongSocketEnabled()) {
        ShennongManager::GetInstance()->Resume();
    }
#endif
#endif
}

shared_ptr<Pipeline> PipelineManager::FindConfigByName(const string& configName) const {
    auto it = mPipelineNameEntityMap.find(configName);
    if (it != mPipelineNameEntityMap.end()) {
        return it->second;
    }
    return nullptr;
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
#if defined(__ENTERPRISE__) && defined(__linux__) && !defined(__ANDROID__)
    if (AppConfig::GetInstance()->GetOpenStreamLog()) {
        StreamLogManager::GetInstance()->Shutdown();
    }
#endif
#if defined(__linux__) && !defined(__ANDROID__)
    ObserverManager::GetInstance()->HoldOn(true);
    ebpf::eBPFServer::GetInstance()->Stop();
#endif
    FileServer::GetInstance()->Stop();
    PrometheusInputRunner::GetInstance()->Stop();

    bool logProcessFlushFlag = false;
    for (int i = 0; !logProcessFlushFlag && i < 500; ++i) {
        logProcessFlushFlag = LogProcess::GetInstance()->FlushOut(10);
    }
    if (!logProcessFlushFlag) {
        LOG_WARNING(sLogger, ("flush process daemon queue", "failed"));
    } else {
        LOG_INFO(sLogger, ("flush process daemon queue", "succeeded"));
    }
    LogProcess::GetInstance()->HoldOn();

    FlushAllBatch();

    LogtailPlugin::GetInstance()->HoldOn(true);

    // TODO: make it common
    FlusherSLS::RecycleResourceIfNotUsed();

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

void PipelineManager::CheckIfInputUpdated(const Json::Value& config,
                                          bool& isInputObserverChanged,
                                          bool& isInputFileChanged,
                                          bool& isInputStreamChanged,
                                          bool& isInputContainerStdioChanged,
                                          bool& isInputPrometheusChanged,
                                          bool& isInputEbpfChanged) {
    string inputType = config["Type"].asString();
    if (inputType == "input_observer_network") {
        isInputObserverChanged = true;
    } else if (inputType == "input_file") {
        isInputFileChanged = true;
    } else if (inputType == "input_stream") {
        isInputStreamChanged = true;
    } else if (inputType == "input_container_stdio") {
        isInputContainerStdioChanged = true;
    } else if (inputType == "input_prometheus") {
        isInputPrometheusChanged = true;
    } else if (inputType == "input_ebpf_processprobe_security" || 
        inputType == "input_ebpf_processprobe_observer" ||
        inputType == "input_ebpf_sockettraceprobe_security" ||
        inputType == "input_ebpf_sockettraceprobe_observer" ||
        inputType == "input_ebpf_fileprobe_security" ||
        inputType == "input_ebpf_profilingprobe_observer") {
        isInputEbpfChanged = true;
    }
}

} // namespace logtail
