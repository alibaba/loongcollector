// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "runner/ProcessorRunner.h"

#include "app_config/AppConfig.h"
#include "batch/TimeoutFlushManager.h"
#include "collection_pipeline/CollectionPipelineManager.h"
#include "common/Flags.h"
#include "go_pipeline/LogtailPlugin.h"
#include "models/EventPool.h"
#include "monitor/AlarmManager.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "protobuf/models/ProtocolConversion.h"
#include "queue/ProcessQueueManager.h"
#include "queue/QueueKeyManager.h"

DEFINE_FLAG_INT32(default_flush_merged_buffer_interval, "default flush merged buffer, seconds", 1);
DEFINE_FLAG_INT32(processor_runner_exit_timeout_sec, "", 60);

DECLARE_FLAG_INT32(max_send_log_group_size);

using namespace std;

namespace logtail {

thread_local uint32_t ProcessorRunner::sThreadNo;
thread_local MetricsRecordRef ProcessorRunner::sMetricsRecordRef;
thread_local CounterPtr ProcessorRunner::sInGroupsCnt;
thread_local CounterPtr ProcessorRunner::sInEventsCnt;
thread_local CounterPtr ProcessorRunner::sInGroupDataSizeBytes;
thread_local IntGaugePtr ProcessorRunner::sLastRunTime;

ProcessorRunner::ProcessorRunner()
    : mThreadCount(AppConfig::GetInstance()->GetProcessThreadCount()), mThreadRes(mThreadCount) {
}

void ProcessorRunner::Init() {
    for (uint32_t threadNo = 0; threadNo < mThreadCount; ++threadNo) {
        mThreadRes[threadNo] = async(launch::async, &ProcessorRunner::Run, this, threadNo);
    }
    mIsFlush = false;
}

void ProcessorRunner::Stop() {
    mIsFlush = true;
    ProcessQueueManager::GetInstance()->Trigger();
    for (uint32_t threadNo = 0; threadNo < mThreadCount; ++threadNo) {
        if (!mThreadRes[threadNo].valid()) {
            continue;
        }
        future_status s = mThreadRes[threadNo].wait_for(chrono::seconds(INT32_FLAG(processor_runner_exit_timeout_sec)));
        if (s == future_status::ready) {
            LOG_INFO(sLogger, ("processor runner", "stopped successfully")("threadNo", threadNo));
        } else {
            LOG_WARNING(sLogger, ("processor runner", "forced to stopped")("threadNo", threadNo));
        }
    }
}

bool ProcessorRunner::PushQueue(QueueKey key, size_t inputIndex, PipelineEventGroup&& group, uint32_t retryTimes) {
    unique_ptr<ProcessQueueItem> item = make_unique<ProcessQueueItem>(std::move(group), inputIndex);
    for (size_t i = 0; i < retryTimes; ++i) {
        if (ProcessQueueManager::GetInstance()->PushQueue(key, std::move(item)) == QueueStatus::OK) {
            return true;
        }
        if (i % 100 == 0) {
            LOG_WARNING(sLogger,
                        ("push attempts to process queue continuously failed for the past second",
                         "retry again")("config", QueueKeyManager::GetInstance()->GetName(key))("input index",
                                                                                                ToString(inputIndex)));
        }
        this_thread::sleep_for(chrono::milliseconds(10));
    }
    group = std::move(item->mEventGroup);
    return false;
}

void ProcessorRunner::Run(uint32_t threadNo) {
    LOG_INFO(sLogger, ("processor runner", "started")("thread no", threadNo));

    // thread local metrics should be initialized in each thread
    sThreadNo = threadNo;
    WriteMetrics::GetInstance()->CreateMetricsRecordRef(
        sMetricsRecordRef,
        MetricCategory::METRIC_CATEGORY_RUNNER,
        {{METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_PROCESSOR},
         {METRIC_LABEL_KEY_THREAD_NO, ToString(threadNo)}});
    sInGroupsCnt = sMetricsRecordRef.CreateCounter(METRIC_RUNNER_IN_EVENT_GROUPS_TOTAL);
    sInEventsCnt = sMetricsRecordRef.CreateCounter(METRIC_RUNNER_IN_EVENTS_TOTAL);
    sInGroupDataSizeBytes = sMetricsRecordRef.CreateCounter(METRIC_RUNNER_IN_SIZE_BYTES);
    sLastRunTime = sMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_LAST_RUN_TIME);
    WriteMetrics::GetInstance()->CommitMetricsRecordRef(sMetricsRecordRef);

    static int32_t lastFlushBatchTime = 0;
    while (true) {
        int32_t curTime = time(nullptr);
        if (threadNo == 0 && curTime - lastFlushBatchTime >= INT32_FLAG(default_flush_merged_buffer_interval)) {
            TimeoutFlushManager::GetInstance()->FlushTimeoutBatch();
            lastFlushBatchTime = curTime;
        }

        SET_GAUGE(sLastRunTime, curTime);
        unique_ptr<ProcessQueueItem> item;
        string configName;
        if (!ProcessQueueManager::GetInstance()->PopItem(threadNo, item, configName)) {
            if (mIsFlush && ProcessQueueManager::GetInstance()->IsAllQueueEmpty()) {
                break;
            }
            ProcessQueueManager::GetInstance()->Wait(100);
            continue;
        }

        ADD_COUNTER(sInEventsCnt, item->mEventGroup.GetEvents().size());
        ADD_COUNTER(sInGroupsCnt, 1);
        ADD_COUNTER(sInGroupDataSizeBytes, item->mEventGroup.DataSize());

        const shared_ptr<CollectionPipeline>& pipeline
            = CollectionPipelineManager::GetInstance()->FindConfigByName(configName);
        if (!pipeline) {
            LOG_INFO(sLogger,
                     ("pipeline not found during processing, perhaps due to config deletion",
                      "discard data")("config", configName));
            continue;
        }

        vector<PipelineEventGroup> eventGroupList;
        eventGroupList.emplace_back(std::move(item->mEventGroup));
        // TODO: use old pipeline input index to find inner processor in new pipeline, maybe cause some issues when
        // there are multiple inputs
        pipeline->Process(eventGroupList, item->mInputIndex);

        if (pipeline->IsFlushingThroughGoPipeline()) {
            for (auto& group : eventGroupList) {
                if (group.GetEvents().empty()) {
                    continue;
                }
                const auto eventType = group.GetEvents()[0]->GetType();
                if (eventType != PipelineEvent::Type::LOG && eventType != PipelineEvent::Type::METRIC
                    && eventType != PipelineEvent::Type::SPAN) {
                    LOG_WARNING(pipeline->GetContext().GetLogger(),
                                ("unsupported event type in go pipeline", "discard data")("config", configName));
                    continue;
                }
                models::PipelineEventGroup pbGroup;
                string errorMsg;
                if (!TransferPipelineEventGroupToPB(group, pbGroup, errorMsg)) {
                    LOG_WARNING(pipeline->GetContext().GetLogger(),
                                ("failed to transfer event group to pb",
                                 errorMsg)("action", "discard data")("config", configName));
                    pipeline->GetContext().GetAlarm().SendAlarmWarning(
                        SERIALIZE_FAIL_ALARM,
                        "failed to transfer event group to pb: " + errorMsg
                            + "\taction: discard data\tconfig: " + configName,
                        pipeline->GetContext().GetRegion(),
                        pipeline->GetContext().GetProjectName(),
                        configName,
                        pipeline->GetContext().GetLogstoreName());
                    continue;
                }
                string res;
                if (!pbGroup.SerializeToString(&res)) {
                    LOG_WARNING(pipeline->GetContext().GetLogger(),
                                ("failed to serialize pipeline event group pb", "discard data")("config", configName));
                    continue;
                }
                LogtailPlugin::GetInstance()->ProcessPipelineEventGroup(
                    pipeline->GetContext().GetConfigName(),
                    res,
                    group.GetMetadata(EventGroupMetaKey::SOURCE_ID).to_string());
            }
        } else {
            pipeline->Send(std::move(eventGroupList));
        }
        pipeline->SubInProcessCnt();

        gThreadedEventPool.CheckGC();
    }
}

} // namespace logtail
