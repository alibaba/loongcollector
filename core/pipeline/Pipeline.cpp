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

#include <cstdint>
#include <utility>

#include "batch/TimeoutFlushManager.h"
#include "common/Flags.h"
#include "common/ParamExtractor.h"
#include "flusher/sls/FlusherSLS.h"
#include "go_pipeline/LogtailPlugin.h"
#include "input/InputFeedbackInterfaceRegistry.h"
#include "plugin/PluginRegistry.h"
#include "processor/ProcessorParseApsaraNative.h"
#include "queue/ProcessQueueManager.h"
#include "queue/QueueKeyManager.h"
#include "queue/SenderQueueManager.h"

DECLARE_FLAG_INT32(default_plugin_log_queue_size);

using namespace std;

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

#ifdef __ENTERPRISE__
    // to send alarm before flusherSLS is built, a temporary object is made, which will be overriden shortly after.
    unique_ptr<FlusherSLS> SLSTmp = make_unique<FlusherSLS>();
    SLSTmp->mProject = config.mProject;
    SLSTmp->mLogstore = config.mLogstore;
    SLSTmp->mRegion = config.mRegion;
    mContext.SetSLSInfo(SLSTmp.get());
#endif

    mPluginID.store(0);
    for (size_t i = 0; i < config.mInputs.size(); ++i) {
        const Json::Value& detail = *config.mInputs[i];
        string name = detail["Type"].asString();
        unique_ptr<InputInstance> input = PluginRegistry::GetInstance()->CreateInput(name, GenNextPluginMeta(false));
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
            if (name == InputFile::sName) {
                inputFile = static_cast<const InputFile*>(mInputs[0]->GetPlugin());
            } else if (name == InputContainerStdio::sName) {
                inputContainerStdio = static_cast<const InputContainerStdio*>(mInputs[0]->GetPlugin());
            }
        } else {
            AddPluginToGoPipeline(detail, "inputs", mGoPipelineWithInput);
        }
        ++mPluginCntMap["inputs"][name];
    }

    for (size_t i = 0; i < config.mProcessors.size(); ++i) {
        string name = (*config.mProcessors[i])["Type"].asString();
        unique_ptr<ProcessorInstance> processor
            = PluginRegistry::GetInstance()->CreateProcessor(name, GenNextPluginMeta(false));
        if (processor) {
            if (!processor->Init(*config.mProcessors[i], mContext)) {
                return false;
            }
            mProcessorLine.emplace_back(std::move(processor));
            // for special treatment of topicformat in apsara mode
            if (i == 0 && name == ProcessorParseApsaraNative::sName) {
                mContext.SetIsFirstProcessorApsaraFlag(true);
            }
        } else {
            if (ShouldAddPluginToGoPipelineWithInput()) {
                AddPluginToGoPipeline(*config.mProcessors[i], "processors", mGoPipelineWithInput);
            } else {
                AddPluginToGoPipeline(*config.mProcessors[i], "processors", mGoPipelineWithoutInput);
            }
        }
        ++mPluginCntMap["processors"][name];
    }

    for (auto detail : config.mAggregators) {
        if (ShouldAddPluginToGoPipelineWithInput()) {
            AddPluginToGoPipeline(*detail, "aggregators", mGoPipelineWithInput);
        } else {
            AddPluginToGoPipeline(*detail, "aggregators", mGoPipelineWithoutInput);
        }
        ++mPluginCntMap["aggregators"][(*detail)["Type"].asString()];
    }

    for (auto detail : config.mFlushers) {
        string name = (*detail)["Type"].asString();
        unique_ptr<FlusherInstance> flusher
            = PluginRegistry::GetInstance()->CreateFlusher(name, GenNextPluginMeta(false));
        if (flusher) {
            Json::Value optionalGoPipeline;
            if (!flusher->Init(*detail, mContext, optionalGoPipeline)) {
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
            if (name == FlusherSLS::sName) {
                hasFlusherSLS = true;
                mContext.SetSLSInfo(static_cast<const FlusherSLS*>(mFlushers.back()->GetPlugin()));
            }
        } else {
            if (ShouldAddPluginToGoPipelineWithInput()) {
                AddPluginToGoPipeline(*detail, "flushers", mGoPipelineWithInput);
            } else {
                AddPluginToGoPipeline(*detail, "flushers", mGoPipelineWithoutInput);
            }
        }
        ++mPluginCntMap["flushers"][name];
    }

    // route is only enabled in native flushing mode, thus the index in config is the same as that in mFlushers
    if (!mRouter.Init(config.mRouter, mContext)) {
        return false;
    }

    for (auto detail : config.mExtensions) {
        if (!mGoPipelineWithInput.isNull()) {
            AddPluginToGoPipeline(*detail, "extensions", mGoPipelineWithInput);
        }
        if (!mGoPipelineWithoutInput.isNull()) {
            AddPluginToGoPipeline(*detail, "extensions", mGoPipelineWithoutInput);
        }
        ++mPluginCntMap["extensions"][(*detail)["Type"].asString()];
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
        uint32_t priority = mContext.GetGlobalConfig().mProcessPriority == 0
            ? ProcessQueueManager::sMaxPriority
            : mContext.GetGlobalConfig().mProcessPriority - 1;
        ProcessQueueManager::GetInstance()->CreateOrUpdateQueue(
            mContext.GetProcessQueueKey(), priority, ProcessQueueManager::QueueType::BOUNDED);

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

    return true;
}

void Pipeline::Start() {
    // TODO: 应该保证指定时间内返回，如果无法返回，将配置放入startDisabled里
    for (const auto& flusher : mFlushers) {
        flusher->Start();
    }

    if (!mGoPipelineWithoutInput.isNull()) {
        // TODO: 加载该Go流水线
    }

    // TODO: 启用Process中改流水线对应的输入队列

    if (!mGoPipelineWithInput.isNull()) {
        // TODO: 加载该Go流水线
    }

    for (const auto& input : mInputs) {
        input->Start();
    }

    LOG_INFO(sLogger, ("pipeline start", "succeeded")("config", mName));
}

void Pipeline::Process(vector<PipelineEventGroup>& logGroupList, size_t inputIndex) {
    for (auto& p : mInputs[inputIndex]->GetInnerProcessors()) {
        p->Process(logGroupList);
    }
    for (auto& p : mProcessorLine) {
        p->Process(logGroupList);
    }
}

bool Pipeline::Send(vector<PipelineEventGroup>&& groupList) {
    bool allSucceeded = true;
    for (auto& group : groupList) {
        auto flusherIdx = mRouter.Route(group);
        for (size_t i = 0; i < flusherIdx.size(); ++i) {
            if (flusherIdx[i] >= mFlushers.size()) {
                LOG_ERROR(
                    sLogger,
                    ("unexpected error", "invalid flusher index")("flusher index", flusherIdx[i])("config", mName));
                allSucceeded = false;
                continue;
            }
            if (i + 1 != flusherIdx.size()) {
                allSucceeded = mFlushers[flusherIdx[i]]->Send(group.Copy()) && allSucceeded;
            } else {
                allSucceeded = mFlushers[flusherIdx[i]]->Send(std::move(group)) && allSucceeded;
            }
        }
    }
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
    // TODO: 应该保证指定时间内返回，如果无法返回，将配置放入stopDisabled里
    for (const auto& input : mInputs) {
        input->Stop(isRemoving);
    }

    if (!mGoPipelineWithInput.isNull()) {
        // TODO: 卸载该Go流水线
    }

    // TODO: 禁用Process中改流水线对应的输入队列

    if (!isRemoving) {
        FlushBatch();
    }

    if (!mGoPipelineWithoutInput.isNull()) {
        // TODO: 卸载该Go流水线
    }

    for (const auto& flusher : mFlushers) {
        flusher->Stop(isRemoving);
    }

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

void Pipeline::AddPluginToGoPipeline(const Json::Value& plugin, const string& module, Json::Value& dst) {
    Json::Value res(Json::objectValue), detail = plugin;
    detail.removeMember("Type");
    res["type"] = plugin["Type"];
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
    // TODO：将下面的代码替换成批量原子Load。
    // note:
    // 目前按照从后往前顺序加载，即便without成功with失败导致without残留在插件系统中，也不会有太大的问题，但最好改成原子的。
    if (!mGoPipelineWithoutInput.isNull()) {
        string content = mGoPipelineWithoutInput.toStyledString();
        if (!LogtailPlugin::GetInstance()->LoadPipeline(mName + "/2",
                                                        content,
                                                        mContext.GetProjectName(),
                                                        mContext.GetLogstoreName(),
                                                        mContext.GetRegion(),
                                                        mContext.GetLogstoreKey())) {
            LOG_ERROR(mContext.GetLogger(),
                      ("failed to init pipeline", "Go pipeline is invalid, see logtail_plugin.LOG for detail")(
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
        if (!LogtailPlugin::GetInstance()->LoadPipeline(mName + "/1",
                                                        content,
                                                        mContext.GetProjectName(),
                                                        mContext.GetLogstoreName(),
                                                        mContext.GetRegion(),
                                                        mContext.GetLogstoreKey())) {
            LOG_ERROR(mContext.GetLogger(),
                      ("failed to init pipeline", "Go pipeline is invalid, see logtail_plugin.LOG for detail")(
                          "Go pipeline num", "1")("Go pipeline content", content)("config", mName));
            LogtailAlarm::GetInstance()->SendAlarm(CATEGORY_CONFIG_ALARM,
                                                   "Go pipeline is invalid, content: " + content + ", config: " + mName,
                                                   mContext.GetProjectName(),
                                                   mContext.GetLogstoreName(),
                                                   mContext.GetRegion());
            return false;
        }
    }
    return true;
}

std::string Pipeline::GetNowPluginID() {
    return std::to_string(mPluginID.load());
}

std::string Pipeline::GenNextPluginID() {
    mPluginID.fetch_add(1);
    return std::to_string(mPluginID.load());
}

PluginInstance::PluginMeta Pipeline::GenNextPluginMeta(bool lastOne) {
    mPluginID.fetch_add(1);
    int32_t childNodeID = mPluginID.load();
    if (lastOne) {
        childNodeID = -1;
    } else {
        childNodeID += 1;
    }
    return PluginInstance::PluginMeta(
        std::to_string(mPluginID.load()), std::to_string(mPluginID.load()), std::to_string(childNodeID));
}

} // namespace logtail
