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

#include "pipeline/Pipeline.h"

#include <chrono>
#include <cstdint>
#include <utility>

#include "common/Flags.h"
#include "common/ParamExtractor.h"
#include "go_pipeline/LogtailPlugin.h"
#include "pipeline/batch/TimeoutFlushManager.h"
#include "pipeline/plugin/PluginRegistry.h"
#include "pipeline/queue/ProcessQueueManager.h"
#include "pipeline/queue/QueueKeyManager.h"
#include "pipeline/queue/SenderQueueManager.h"
#include "plugin/flusher/sls/FlusherSLS.h"
#include "plugin/input/InputFeedbackInterfaceRegistry.h"
#include "plugin/processor/ProcessorParseApsaraNative.h"

DECLARE_FLAG_INT32(default_plugin_log_queue_size);

using namespace std;

namespace {
class AggregatorDefaultConfig {
public:
    static AggregatorDefaultConfig& Instance() {
        static AggregatorDefaultConfig instance;
        return instance;
    }

    Json::Value* GetJsonConfig() { return &aggregatorDefault; }

private:
    Json::Value aggregatorDefault;
    AggregatorDefaultConfig() { aggregatorDefault["Type"] = "aggregator_default"; }

    AggregatorDefaultConfig(AggregatorDefaultConfig const&) = delete;
    void operator=(AggregatorDefaultConfig const&) = delete;
};
} // namespace

namespace logtail {

void AddExtendedGlobalParamToGoPipeline(const Json::Value& extendedParams, Json::Value& pipeline) {
    if (!pipeline.isNull()) {
        Json::Value& global = pipeline["global"];
        for (auto itr = extendedParams.begin(); itr != extendedParams.end(); itr++) {
            global[itr.name()] = *itr;
        }
    }
}

bool Pipeline::Init(PipelineConfig&& config) {
    mName = config.mName;
    mConfig = std::move(config.mDetail);
    mContext.SetConfigName(mName);
    mContext.SetCreateTime(config.mCreateTime);
    mContext.SetPipeline(*this);
    mContext.SetIsFirstProcessorJsonFlag(config.mIsFirstProcessorJson);

    // for special treatment below
    const InputFile* inputFile = nullptr;
    const InputContainerStdio* inputContainerStdio = nullptr;
    bool hasFlusherSLS = false;

    // to send alarm and init MetricsRecord before flusherSLS is built, a temporary object is made, which will be
    unique_ptr<FlusherSLS> SLSTmp = make_unique<FlusherSLS>();
    if (!config.mProject.empty()) {
        SLSTmp->mProject = config.mProject;
        SLSTmp->mLogstore = config.mLogstore;
        SLSTmp->mRegion = config.mRegion;
        mContext.SetSLSInfo(SLSTmp.get());
    }

    mPluginID.store(0);
    mInProcessCnt.store(0);
    for (size_t i = 0; i < config.mInputs.size(); ++i) {
        const Json::Value& detail = *config.mInputs[i];
        string pluginType = detail["Type"].asString();
        unique_ptr<InputInstance> input
            = PluginRegistry::GetInstance()->CreateInput(pluginType, GenNextPluginMeta(false));
        if (input) {
            Json::Value optionalGoPipeline;
            if (!input->Init(detail, mContext, i, optionalGoPipeline)) {
                return false;
            }
            mInputs.emplace_back(std::move(input));
            if (!optionalGoPipeline.isNull()) {
                MergeGoPipeline(optionalGoPipeline, mGoPipelineWithInput);
            }
            // for special treatment below
            if (pluginType == InputFile::sName) {
                inputFile = static_cast<const InputFile*>(mInputs[0]->GetPlugin());
            } else if (pluginType == InputContainerStdio::sName) {
                inputContainerStdio = static_cast<const InputContainerStdio*>(mInputs[0]->GetPlugin());
            }
        } else {
            AddPluginToGoPipeline(pluginType, detail, "inputs", mGoPipelineWithInput);
        }
        ++mPluginCntMap["inputs"][pluginType];
    }

    for (size_t i = 0; i < config.mProcessors.size(); ++i) {
        const Json::Value& detail = *config.mProcessors[i];
        string pluginType = detail["Type"].asString();
        unique_ptr<ProcessorInstance> processor
            = PluginRegistry::GetInstance()->CreateProcessor(pluginType, GenNextPluginMeta(false));
        if (processor) {
            if (!processor->Init(detail, mContext)) {
                return false;
            }
            mProcessorLine.emplace_back(std::move(processor));
            // for special treatment of topicformat in apsara mode
            if (i == 0 && pluginType == ProcessorParseApsaraNative::sName) {
                mContext.SetIsFirstProcessorApsaraFlag(true);
            }
        } else {
            if (ShouldAddPluginToGoPipelineWithInput()) {
                AddPluginToGoPipeline(pluginType, detail, "processors", mGoPipelineWithInput);
            } else {
                AddPluginToGoPipeline(pluginType, detail, "processors", mGoPipelineWithoutInput);
            }
        }
        ++mPluginCntMap["processors"][pluginType];
    }

    if (config.mAggregators.empty() && config.IsFlushingThroughGoPipelineExisted()) {
        // an aggregator_default plugin will be add to go pipeline when mAggregators is empty and need to send go data
        // to cpp flusher.
        config.mAggregators.push_back(AggregatorDefaultConfig::Instance().GetJsonConfig());
    }
    for (size_t i = 0; i < config.mAggregators.size(); ++i) {
        const Json::Value& detail = *config.mAggregators[i];
        string pluginType = detail["Type"].asString();
        GenNextPluginMeta(false);
        if (ShouldAddPluginToGoPipelineWithInput()) {
            AddPluginToGoPipeline(pluginType, detail, "aggregators", mGoPipelineWithInput);
        } else {
            AddPluginToGoPipeline(pluginType, detail, "aggregators", mGoPipelineWithoutInput);
        }
        ++mPluginCntMap["aggregators"][pluginType];
    }

    for (size_t i = 0; i < config.mFlushers.size(); ++i) {
        const Json::Value& detail = *config.mFlushers[i];
        string pluginType = detail["Type"].asString();
        unique_ptr<FlusherInstance> flusher
            = PluginRegistry::GetInstance()->CreateFlusher(pluginType, GenNextPluginMeta(false));
        if (flusher) {
            Json::Value optionalGoPipeline;
            if (!flusher->Init(detail, mContext, optionalGoPipeline)) {
                return false;
            }
            mFlushers.emplace_back(std::move(flusher));
            if (!optionalGoPipeline.isNull() && config.ShouldNativeFlusherConnectedByGoPipeline()) {
                if (ShouldAddPluginToGoPipelineWithInput()) {
                    MergeGoPipeline(optionalGoPipeline, mGoPipelineWithInput);
                } else {
                    MergeGoPipeline(optionalGoPipeline, mGoPipelineWithoutInput);
                }
            }
            if (pluginType == FlusherSLS::sName) {
                hasFlusherSLS = true;
                mContext.SetSLSInfo(static_cast<const FlusherSLS*>(mFlushers.back()->GetPlugin()));
            }
        } else {
            if (ShouldAddPluginToGoPipelineWithInput()) {
                AddPluginToGoPipeline(pluginType, detail, "flushers", mGoPipelineWithInput);
            } else {
                AddPluginToGoPipeline(pluginType, detail, "flushers", mGoPipelineWithoutInput);
            }
        }
        ++mPluginCntMap["flushers"][pluginType];
    }

    // route is only enabled in native flushing mode, thus the index in config is the same as that in mFlushers
    if (!mRouter.Init(config.mRouter, mContext)) {
        return false;
    }

    for (size_t i = 0; i < config.mExtensions.size(); ++i) {
        const Json::Value& detail = *config.mExtensions[i];
        string pluginType = detail["Type"].asString();
        GenNextPluginMeta(false);
        if (!mGoPipelineWithInput.isNull()) {
            AddPluginToGoPipeline(pluginType, detail, "extensions", mGoPipelineWithInput);
        }
        if (!mGoPipelineWithoutInput.isNull()) {
            AddPluginToGoPipeline(pluginType, detail, "extensions", mGoPipelineWithoutInput);
        }
        ++mPluginCntMap["extensions"][pluginType];
    }

    // global module must be initialized at last, since native input or flusher plugin may generate global param in Go
    // pipeline, which should be overriden by explicitly provided global module.
    if (config.mGlobal) {
        Json::Value extendedParams;
        if (!mContext.InitGlobalConfig(*config.mGlobal, extendedParams)) {
            return false;
        }
        AddExtendedGlobalParamToGoPipeline(extendedParams, mGoPipelineWithInput);
        AddExtendedGlobalParamToGoPipeline(extendedParams, mGoPipelineWithoutInput);
    }
    CopyNativeGlobalParamToGoPipeline(mGoPipelineWithInput);
    CopyNativeGlobalParamToGoPipeline(mGoPipelineWithoutInput);

    // mandatory override global.DefaultLogQueueSize in Go pipeline when input_file and Go processing coexist.
    if ((inputFile != nullptr || inputContainerStdio != nullptr) && IsFlushingThroughGoPipeline()) {
        mGoPipelineWithoutInput["global"]["DefaultLogQueueSize"]
            = Json::Value(INT32_FLAG(default_plugin_log_queue_size));
    }

    // special treatment for exactly once
    if (inputFile && inputFile->mExactlyOnceConcurrency > 0) {
        if (mInputs.size() > 1) {
            PARAM_ERROR_RETURN(mContext.GetLogger(),
                               mContext.GetAlarm(),
                               "exactly once enabled when input other than input_file is given",
                               noModule,
                               mName,
                               mContext.GetProjectName(),
                               mContext.GetLogstoreName(),
                               mContext.GetRegion());
        }
        if (mFlushers.size() > 1 || !hasFlusherSLS) {
            PARAM_ERROR_RETURN(mContext.GetLogger(),
                               mContext.GetAlarm(),
                               "exactly once enabled when flusher other than flusher_sls is given",
                               noModule,
                               mName,
                               mContext.GetProjectName(),
                               mContext.GetLogstoreName(),
                               mContext.GetRegion());
        }
        if (IsFlushingThroughGoPipeline()) {
            PARAM_ERROR_RETURN(mContext.GetLogger(),
                               mContext.GetAlarm(),
                               "exactly once enabled when not in native mode",
                               noModule,
                               mName,
                               mContext.GetProjectName(),
                               mContext.GetLogstoreName(),
                               mContext.GetRegion());
        }
    }

#ifndef APSARA_UNIT_TEST_MAIN
    if (!LoadGoPipelines()) {
        return false;
    }
#endif

    // Process queue, not generated when exactly once is enabled
    if (!inputFile || inputFile->mExactlyOnceConcurrency == 0) {
        if (mContext.GetProcessQueueKey() == -1) {
            mContext.SetProcessQueueKey(QueueKeyManager::GetInstance()->GetKey(mName));
        }

        // TODO: for go input, we currently assume bounded process queue
        bool isInputSupportAck = mInputs.empty() ? true : mInputs[0]->SupportAck();
        for (auto& input : mInputs) {
            if (input->SupportAck() != isInputSupportAck) {
                PARAM_ERROR_RETURN(mContext.GetLogger(),
                                   mContext.GetAlarm(),
                                   "not all inputs' ack support are the same",
                                   noModule,
                                   mName,
                                   mContext.GetProjectName(),
                                   mContext.GetLogstoreName(),
                                   mContext.GetRegion());
            }
        }
        uint32_t priority = mContext.GetGlobalConfig().mProcessPriority == 0
            ? ProcessQueueManager::sMaxPriority
            : mContext.GetGlobalConfig().mProcessPriority - 1;
        if (isInputSupportAck) {
            ProcessQueueManager::GetInstance()->CreateOrUpdateBoundedQueue(
                mContext.GetProcessQueueKey(), priority, mContext);
        } else {
            ProcessQueueManager::GetInstance()->CreateOrUpdateCircularQueue(
                mContext.GetProcessQueueKey(), priority, 1024, mContext);
        }


        unordered_set<FeedbackInterface*> feedbackSet;
        for (const auto& input : mInputs) {
            FeedbackInterface* feedback
                = InputFeedbackInterfaceRegistry::GetInstance()->GetFeedbackInterface(input->Name());
            if (feedback != nullptr) {
                feedbackSet.insert(feedback);
            }
        }
        ProcessQueueManager::GetInstance()->SetFeedbackInterface(
            mContext.GetProcessQueueKey(), vector<FeedbackInterface*>(feedbackSet.begin(), feedbackSet.end()));

        vector<BoundedSenderQueueInterface*> senderQueues;
        for (const auto& flusher : mFlushers) {
            senderQueues.push_back(SenderQueueManager::GetInstance()->GetQueue(flusher->GetQueueKey()));
        }
        ProcessQueueManager::GetInstance()->SetDownStreamQueues(mContext.GetProcessQueueKey(), std::move(senderQueues));
    }

    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
        mMetricsRecordRef,
        {{METRIC_LABEL_KEY_PROJECT, mContext.GetProjectName()},
         {METRIC_LABEL_KEY_PIPELINE_NAME, mName},
         {METRIC_LABEL_KEY_METRIC_CATEGORY, METRIC_LABEL_KEY_METRIC_CATEGORY_PIPELINE}});
    mStartTime = mMetricsRecordRef.CreateIntGauge(METRIC_PIPELINE_START_TIME);
    mProcessorsInEventsTotal = mMetricsRecordRef.CreateCounter(METRIC_PIPELINE_PROCESSORS_IN_EVENTS_TOTAL);
    mProcessorsInGroupsTotal = mMetricsRecordRef.CreateCounter(METRIC_PIPELINE_PROCESSORS_IN_EVENT_GROUPS_TOTAL);
    mProcessorsInSizeBytes = mMetricsRecordRef.CreateCounter(METRIC_PIPELINE_PROCESSORS_IN_SIZE_BYTES);
    mProcessorsTotalProcessTimeMs
        = mMetricsRecordRef.CreateTimeCounter(METRIC_PIPELINE_PROCESSORS_TOTAL_PROCESS_TIME_MS);
    mFlushersInGroupsTotal = mMetricsRecordRef.CreateCounter(METRIC_PIPELINE_FLUSHERS_IN_EVENT_GROUPS_TOTAL);
    mFlushersInEventsTotal = mMetricsRecordRef.CreateCounter(METRIC_PIPELINE_FLUSHERS_IN_EVENTS_TOTAL);
    mFlushersInSizeBytes = mMetricsRecordRef.CreateCounter(METRIC_PIPELINE_FLUSHERS_IN_SIZE_BYTES);
    mFlushersTotalPackageTimeMs = mMetricsRecordRef.CreateTimeCounter(METRIC_PIPELINE_FLUSHERS_TOTAL_PACKAGE_TIME_MS);

    return true;
}

void Pipeline::Start() {
#ifndef APSARA_UNIT_TEST_MAIN
    // TODO: 应该保证指定时间内返回，如果无法返回，将配置放入startDisabled里
    for (const auto& flusher : mFlushers) {
        flusher->Start();
    }

    if (!mGoPipelineWithoutInput.isNull()) {
        LogtailPlugin::GetInstance()->Start(GetConfigNameOfGoPipelineWithoutInput());
    }

    ProcessQueueManager::GetInstance()->EnablePop(mName);

    if (!mGoPipelineWithInput.isNull()) {
        LogtailPlugin::GetInstance()->Start(GetConfigNameOfGoPipelineWithInput());
    }

    for (const auto& input : mInputs) {
        input->Start();
    }

    mStartTime->Set(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count());
#endif
    LOG_INFO(sLogger, ("pipeline start", "succeeded")("config", mName));
}

void Pipeline::Process(vector<PipelineEventGroup>& logGroupList, size_t inputIndex) {
    for (const auto& logGroup : logGroupList) {
        mProcessorsInEventsTotal->Add(logGroup.GetEvents().size());
        mProcessorsInSizeBytes->Add(logGroup.DataSize());
    }
    mProcessorsInGroupsTotal->Add(logGroupList.size());

    auto before = chrono::system_clock::now();
    for (auto& p : mInputs[inputIndex]->GetInnerProcessors()) {
        p->Process(logGroupList);
    }
    for (auto& p : mProcessorLine) {
        p->Process(logGroupList);
    }
    mProcessorsTotalProcessTimeMs->Add(chrono::system_clock::now() - before);
}

bool Pipeline::Send(vector<PipelineEventGroup>&& groupList) {
    for (const auto& group : groupList) {
        mFlushersInEventsTotal->Add(group.GetEvents().size());
        mFlushersInSizeBytes->Add(group.DataSize());
    }
    mFlushersInGroupsTotal->Add(groupList.size());

    auto before = chrono::system_clock::now();
    bool allSucceeded = true;
    for (auto& group : groupList) {
        if (group.GetEvents().empty()) {
            LOG_DEBUG(sLogger, ("empty event group", "discard")("config", mName));
            continue;
        }
        auto res = mRouter.Route(group);
        for (auto& item : res) {
            if (item.first >= mFlushers.size()) {
                LOG_ERROR(sLogger,
                          ("unexpected error", "invalid flusher index")("flusher index", item.first)("config", mName));
                allSucceeded = false;
                continue;
            }
            allSucceeded = mFlushers[item.first]->Send(std::move(item.second)) && allSucceeded;
        }
    }
    mFlushersTotalPackageTimeMs->Add(chrono::system_clock::now() - before);
    return allSucceeded;
}

bool Pipeline::FlushBatch() {
    bool allSucceeded = true;
    for (auto& flusher : mFlushers) {
        allSucceeded = flusher->FlushAll() && allSucceeded;
    }
    TimeoutFlushManager::GetInstance()->ClearRecords(mName);
    return allSucceeded;
}

void Pipeline::Stop(bool isRemoving) {
#ifndef APSARA_UNIT_TEST_MAIN
    // TODO: 应该保证指定时间内返回，如果无法返回，将配置放入stopDisabled里
    for (const auto& input : mInputs) {
        input->Stop(isRemoving);
    }

    if (!mGoPipelineWithInput.isNull()) {
        // Go pipeline `Stop` will stop and delete
        LogtailPlugin::GetInstance()->Stop(GetConfigNameOfGoPipelineWithInput(), isRemoving);
    }

    ProcessQueueManager::GetInstance()->DisablePop(mName, isRemoving);
    WaitAllItemsInProcessFinished();

    FlushBatch();

    if (!mGoPipelineWithoutInput.isNull()) {
        // Go pipeline `Stop` will stop and delete
        LogtailPlugin::GetInstance()->Stop(GetConfigNameOfGoPipelineWithoutInput(), isRemoving);
    }

    for (const auto& flusher : mFlushers) {
        flusher->Stop(isRemoving);
    }
#endif
    LOG_INFO(sLogger, ("pipeline stop", "succeeded")("config", mName));
}

void Pipeline::RemoveProcessQueue() const {
    ProcessQueueManager::GetInstance()->DeleteQueue(mContext.GetProcessQueueKey());
}

void Pipeline::MergeGoPipeline(const Json::Value& src, Json::Value& dst) {
    for (auto itr = src.begin(); itr != src.end(); ++itr) {
        if (itr->isArray()) {
            Json::Value& module = dst[itr.name()];
            for (auto it = itr->begin(); it != itr->end(); ++it) {
                module.append(*it);
            }
        } else if (itr->isObject()) {
            Json::Value& module = dst[itr.name()];
            for (auto it = itr->begin(); it != itr->end(); ++it) {
                module[it.name()] = *it;
            }
        }
    }
}

std::string Pipeline::GenPluginTypeWithID(std::string pluginType, std::string pluginID) {
    return pluginType + "/" + pluginID;
}

// Rule: pluginTypeWithID=pluginType/pluginID#pluginPriority.
void Pipeline::AddPluginToGoPipeline(const string& pluginType,
                                     const Json::Value& plugin,
                                     const string& module,
                                     Json::Value& dst) {
    Json::Value res(Json::objectValue), detail = plugin;
    detail.removeMember("Type");
    res["type"] = GenPluginTypeWithID(pluginType, GetNowPluginID());
    res["detail"] = detail;
    dst[module].append(res);
}

void Pipeline::CopyNativeGlobalParamToGoPipeline(Json::Value& pipeline) {
    if (!pipeline.isNull()) {
        Json::Value& global = pipeline["global"];
        global["EnableTimestampNanosecond"] = mContext.GetGlobalConfig().mEnableTimestampNanosecond;
        global["UsingOldContentTag"] = mContext.GetGlobalConfig().mUsingOldContentTag;
    }
}

bool Pipeline::LoadGoPipelines() const {
    if (!mGoPipelineWithoutInput.isNull()) {
        string content = mGoPipelineWithoutInput.toStyledString();
        if (!LogtailPlugin::GetInstance()->LoadPipeline(GetConfigNameOfGoPipelineWithoutInput(),
                                                        content,
                                                        mContext.GetProjectName(),
                                                        mContext.GetLogstoreName(),
                                                        mContext.GetRegion(),
                                                        mContext.GetLogstoreKey())) {
            LOG_ERROR(mContext.GetLogger(),
                      ("failed to init pipeline", "Go pipeline is invalid, see go_plugin.LOG for detail")(
                          "Go pipeline num", "2")("Go pipeline content", content)("config", mName));
            LogtailAlarm::GetInstance()->SendAlarm(CATEGORY_CONFIG_ALARM,
                                                   "Go pipeline is invalid, content: " + content + ", config: " + mName,
                                                   mContext.GetProjectName(),
                                                   mContext.GetLogstoreName(),
                                                   mContext.GetRegion());
            return false;
        }
    }
    if (!mGoPipelineWithInput.isNull()) {
        string content = mGoPipelineWithInput.toStyledString();
        if (!LogtailPlugin::GetInstance()->LoadPipeline(GetConfigNameOfGoPipelineWithInput(),
                                                        content,
                                                        mContext.GetProjectName(),
                                                        mContext.GetLogstoreName(),
                                                        mContext.GetRegion(),
                                                        mContext.GetLogstoreKey())) {
            LOG_ERROR(mContext.GetLogger(),
                      ("failed to init pipeline", "Go pipeline is invalid, see go_plugin.LOG for detail")(
                          "Go pipeline num", "1")("Go pipeline content", content)("config", mName));
            LogtailAlarm::GetInstance()->SendAlarm(CATEGORY_CONFIG_ALARM,
                                                   "Go pipeline is invalid, content: " + content + ", config: " + mName,
                                                   mContext.GetProjectName(),
                                                   mContext.GetLogstoreName(),
                                                   mContext.GetRegion());
            if (!mGoPipelineWithoutInput.isNull()) {
                LogtailPlugin::GetInstance()->UnloadPipeline(GetConfigNameOfGoPipelineWithoutInput());
            }
            return false;
        }
    }
    return true;
}

std::string Pipeline::GetNowPluginID() {
    return std::to_string(mPluginID.load());
}

PluginInstance::PluginMeta Pipeline::GenNextPluginMeta(bool lastOne) {
    mPluginID.fetch_add(1);
    return PluginInstance::PluginMeta(std::to_string(mPluginID.load()));
}

void Pipeline::WaitAllItemsInProcessFinished() {
    uint64_t startTime = GetCurrentTimeInMilliSeconds();
    bool alarmOnce = false;
    while (mInProcessCnt.load() != 0) {
        this_thread::sleep_for(chrono::milliseconds(100)); // 100ms
        uint64_t duration = GetCurrentTimeInMilliSeconds() - startTime;
        if (!alarmOnce && duration > 10000) { // 10s
            LOG_ERROR(sLogger, ("pipeline stop", "too slow")("config", mName)("cost", duration));
            LogtailAlarm::GetInstance()->SendAlarm(CONFIG_UPDATE_ALARM,
                                                   string("pipeline stop too slow, config: ") + mName
                                                       + "; cost:" + std::to_string(duration),
                                                   mContext.GetProjectName(),
                                                   mContext.GetLogstoreName(),
                                                   mContext.GetRegion());
            alarmOnce = true;
        }
    }
}

} // namespace logtail
