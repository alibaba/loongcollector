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
#include "common/Flags.h"
#include "go_pipeline/LogtailPlugin.h"
#include "models/EventPool.h"
#include "monitor/AlarmManager.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "pipeline/PipelineManager.h"
#include "queue/ExactlyOnceQueueManager.h"
#include "queue/ProcessQueueManager.h"
#include "queue/QueueKeyManager.h"

DEFINE_FLAG_INT32(default_flush_merged_buffer_interval, "default flush merged buffer, seconds", 1);
DEFINE_FLAG_INT32(processor_runner_exit_timeout_secs, "", 60);

DECLARE_FLAG_INT32(max_send_log_group_size);

using namespace std;

namespace logtail {

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
}

void ProcessorRunner::Stop() {
    mIsFlush = true;
    ProcessQueueManager::GetInstance()->Trigger();
    for (uint32_t threadNo = 0; threadNo < mThreadCount; ++threadNo) {
        if (!mThreadRes[threadNo].valid()) {
            continue;
        }
        future_status s
            = mThreadRes[threadNo].wait_for(chrono::seconds(INT32_FLAG(processor_runner_exit_timeout_secs)));
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
        if (ProcessQueueManager::GetInstance()->PushQueue(key, std::move(item)) == 0) {
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
    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
        sMetricsRecordRef,
        MetricCategory::METRIC_CATEGORY_RUNNER,
        {{METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_PROCESSOR},
         {METRIC_LABEL_KEY_THREAD_NO, ToString(threadNo)}});
    sInGroupsCnt = sMetricsRecordRef.CreateCounter(METRIC_RUNNER_IN_EVENT_GROUPS_TOTAL);
    sInEventsCnt = sMetricsRecordRef.CreateCounter(METRIC_RUNNER_IN_EVENTS_TOTAL);
    sInGroupDataSizeBytes = sMetricsRecordRef.CreateCounter(METRIC_RUNNER_IN_SIZE_BYTES);
    sLastRunTime = sMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_LAST_RUN_TIME);

    static int32_t lastFlushBatchTime = 0;
    while (true) {
        int32_t curTime = time(nullptr);
        if (threadNo == 0 && curTime - lastFlushBatchTime >= INT32_FLAG(default_flush_merged_buffer_interval)) {
            TimeoutFlushManager::GetInstance()->FlushTimeoutBatch();
            lastFlushBatchTime = curTime;
        }

        sLastRunTime->Set(curTime);
        unique_ptr<ProcessQueueItem> item;
        string configName;
        if (!ProcessQueueManager::GetInstance()->PopItem(threadNo, item, configName)) {
            if (mIsFlush && ProcessQueueManager::GetInstance()->IsAllQueueEmpty()) {
                break;
            }
            ProcessQueueManager::GetInstance()->Wait(100);
            continue;
        }

        sInEventsCnt->Add(item->mEventGroup.GetEvents().size());
        sInGroupsCnt->Add(1);
        sInGroupDataSizeBytes->Add(item->mEventGroup.DataSize());

        shared_ptr<Pipeline>& pipeline = item->mPipeline;
        bool hasOldPipeline = pipeline != nullptr;
        if (!hasOldPipeline) {
            pipeline = PipelineManager::GetInstance()->FindConfigByName(configName);
        }
        if (!pipeline) {
            LOG_INFO(sLogger,
                     ("pipeline not found during processing, perhaps due to config deletion",
                      "discard data")("config", configName));
            continue;
        }

        bool isLog = !item->mEventGroup.GetEvents().empty() && item->mEventGroup.GetEvents()[0].Is<LogEvent>();

        vector<PipelineEventGroup> eventGroupList;
        eventGroupList.emplace_back(std::move(item->mEventGroup));
        pipeline->Process(eventGroupList, item->mInputIndex);
        // if the pipeline is updated, the pointer will be released, so we need to update it to the new pipeline
        if (hasOldPipeline) {
            pipeline = PipelineManager::GetInstance()->FindConfigByName(configName);
            if (!pipeline) {
                LOG_INFO(sLogger,
                         ("pipeline not found during processing, perhaps due to config deletion",
                          "discard data")("config", configName));
                continue;
            }
        }

        if (pipeline->IsFlushingThroughGoPipeline()) {
            // TODO:
            // 1. allow all event types to be sent to Go pipelines
            // 2. use event group protobuf instead
            if (isLog) {
                for (auto& group : eventGroupList) {
                    string res, errorMsg;
                    if (!Serialize(group,
                                   pipeline->GetContext().GetGlobalConfig().mEnableTimestampNanosecond,
                                   pipeline->GetContext().GetLogstoreName(),
                                   res,
                                   errorMsg)) {
                        LOG_WARNING(pipeline->GetContext().GetLogger(),
                                    ("failed to serialize event group",
                                     errorMsg)("action", "discard data")("config", configName));
                        pipeline->GetContext().GetAlarm().SendAlarm(SERIALIZE_FAIL_ALARM,
                                                                    "failed to serialize event group: " + errorMsg
                                                                        + "\taction: discard data\tconfig: "
                                                                        + configName,
                                                                    pipeline->GetContext().GetProjectName(),
                                                                    pipeline->GetContext().GetLogstoreName(),
                                                                    pipeline->GetContext().GetRegion());
                        continue;
                    }
                    LogtailPlugin::GetInstance()->ProcessLogGroup(
                        pipeline->GetContext().GetConfigName(),
                        res,
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID).to_string());
                }
            }
        } else {
            pipeline->Send(std::move(eventGroupList));
        }
        pipeline->SubInProcessCnt();

        gThreadedEventPool.CheckGC();
    }
}

bool ProcessorRunner::Serialize(
    const PipelineEventGroup& group, bool enableNanosecond, const string& logstore, string& res, string& errorMsg) {
    sls_logs::LogGroup logGroup;
    for (const auto& e : group.GetEvents()) {
        if (e.Is<LogEvent>()) {
            const auto& logEvent = e.Cast<LogEvent>();
            auto log = logGroup.add_logs();
            for (const auto& kv : logEvent) {
                auto contPtr = log->add_contents();
                contPtr->set_key(kv.first.to_string());
                contPtr->set_value(kv.second.to_string());
            }
            log->set_time(logEvent.GetTimestamp());
            if (enableNanosecond && logEvent.GetTimestampNanosecond()) {
                log->set_time_ns(logEvent.GetTimestampNanosecond().value());
            }
        } else {
            errorMsg = "unsupported event type in event group";
            return false;
        }
    }
    for (const auto& tag : group.GetTags()) {
        if (tag.first == LOG_RESERVED_KEY_TOPIC) {
            logGroup.set_topic(tag.second.to_string());
        } else {
            auto logTag = logGroup.add_logtags();
            logTag->set_key(tag.first.to_string());
            logTag->set_value(tag.second.to_string());
        }
    }
    logGroup.set_category(logstore);
    size_t size = logGroup.ByteSizeLong();
    if (static_cast<int32_t>(size) > INT32_FLAG(max_send_log_group_size)) {
        errorMsg = "log group exceeds size limit\tgroup size: " + ToString(size)
            + "\tsize limit: " + ToString(INT32_FLAG(max_send_log_group_size));
        return false;
    }
    res = logGroup.SerializeAsString();
    return true;
}

} // namespace logtail
