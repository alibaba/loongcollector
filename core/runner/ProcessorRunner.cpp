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

#include <shared_mutex>

#include "app_config/AppConfig.h"
#include "batch/TimeoutFlushManager.h"
#include "common/Flags.h"
#include "go_pipeline/LogtailPlugin.h"
#include "monitor/LogFileProfiler.h"
#include "monitor/LogtailAlarm.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "pipeline/PipelineManager.h"
#include "queue/ExactlyOnceQueueManager.h"
#include "queue/ProcessQueueManager.h"
#include "queue/QueueKeyManager.h"

DECLARE_FLAG_INT32(max_send_log_group_size);

using namespace std;

#if defined(_MSC_VER)
// On Windows, if Chinese config base path is used, the log path will be converted to GBK,
// so the __tag__.__path__ have to be converted back to UTF8 to avoid bad display.
// Note: enable this will spend CPU to do transformation.
DEFINE_FLAG_BOOL(enable_chinese_tag_path, "Enable Chinese __tag__.__path__", true);
#endif
DEFINE_FLAG_INT32(default_flush_merged_buffer_interval, "default flush merged buffer, seconds", 1);

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
        future_status s = mThreadRes[threadNo].wait_for(chrono::seconds(1));
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
    return false;
}

void ProcessorRunner::Run(uint32_t threadNo) {
    LOG_INFO(sLogger, ("processor runner", "started")("threadNo", threadNo));

    // thread local metrics should be initialized in each thread
    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
        sMetricsRecordRef, {{METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_PROCESSOR}, {"thread_no", ToString(threadNo)}});
    sInGroupsCnt = sMetricsRecordRef.CreateCounter(METRIC_RUNNER_IN_EVENT_GROUPS_TOTAL);
    sInEventsCnt = sMetricsRecordRef.CreateCounter(METRIC_RUNNER_IN_EVENTS_TOTAL);
    sInGroupDataSizeBytes = sMetricsRecordRef.CreateCounter(METRIC_RUNNER_IN_SIZE_BYTES);
    sLastRunTime = sMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_LAST_RUN_TIME);

    static int32_t lastMergeTime = 0;
    while (true) {
        int32_t curTime = time(NULL);
        if (threadNo == 0 && curTime - lastMergeTime >= INT32_FLAG(default_flush_merged_buffer_interval)) {
            TimeoutFlushManager::GetInstance()->FlushTimeoutBatch();
            lastMergeTime = curTime;
        }

        {
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

            shared_ptr<Pipeline> pipeline = item->mPipeline;
            if (!pipeline) {
                pipeline = PipelineManager::GetInstance()->FindConfigByName(configName);
            }
            if (!pipeline) {
                LOG_INFO(sLogger,
                         ("pipeline not found during processing, perhaps due to config deletion",
                          "discard data")("config", configName));
                continue;
            }

            // record profile, must be placed here since readbytes info exists only before processing
            auto& processProfile = pipeline->GetContext().GetProcessProfile();
            ProcessProfile profile = processProfile;
            bool isLog = false;
            if (!item->mEventGroup.GetEvents().empty() && item->mEventGroup.GetEvents()[0].Is<LogEvent>()) {
                isLog = true;
                profile.readBytes = item->mEventGroup.GetEvents()[0].Cast<LogEvent>().GetPosition().second
                    + 1; // may not be accurate if input is not utf8
            }
            processProfile.Reset();

            int32_t startTime = (int32_t)time(NULL);
            vector<PipelineEventGroup> eventGroupList;
            eventGroupList.emplace_back(std::move(item->mEventGroup));
            pipeline->Process(eventGroupList, item->mInputIndex);
            int32_t elapsedTime = (int32_t)time(NULL) - startTime;
            if (elapsedTime > 1) {
                LOG_WARNING(pipeline->GetContext().GetLogger(),
                            ("event processing took too long, elapsed time", ToString(elapsedTime) + "s")("config",
                                                                                                          configName));
                pipeline->GetContext().GetAlarm().SendAlarm(PROCESS_TOO_SLOW_ALARM,
                                                            string("event processing took too long, elapsed time: ")
                                                                + ToString(elapsedTime) + "s\tconfig: " + configName,
                                                            pipeline->GetContext().GetProjectName(),
                                                            pipeline->GetContext().GetLogstoreName(),
                                                            pipeline->GetContext().GetRegion());
            }

            if (pipeline->IsFlushingThroughGoPipeline()) {
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
                if (isLog) {
                    string convertedPath = eventGroupList[0].GetMetadata(EventGroupMetaKey::LOG_FILE_PATH).to_string();
                    string hostLogPath
                        = eventGroupList[0].GetMetadata(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED).to_string();
#if defined(_MSC_VER)
                    if (BOOL_FLAG(enable_chinese_tag_path)) {
                        convertedPath = EncodingConverter::GetInstance()->FromACPToUTF8(convertedPath);
                        hostLogPath = EncodingConverter::GetInstance()->FromACPToUTF8(hostLogPath);
                    }
#endif
                    LogFileProfiler::GetInstance()->AddProfilingData(
                        pipeline->Name(),
                        pipeline->GetContext().GetRegion(),
                        pipeline->GetContext().GetProjectName(),
                        pipeline->GetContext().GetLogstoreName(),
                        convertedPath,
                        hostLogPath,
                        vector<sls_logs::LogTag>(), // warning: this cannot be recovered!
                        profile.readBytes,
                        profile.skipBytes,
                        profile.splitLines,
                        profile.parseFailures,
                        profile.regexMatchFailures,
                        profile.parseTimeFailures,
                        profile.historyFailures,
                        0,
                        ""); // TODO: I don't think errorLine is useful
                }
                pipeline->Send(std::move(eventGroupList));
            }
            pipeline->SubInProcessCnt();
        }
    }
    LOG_WARNING(sLogger, ("ProcessorRunnerThread", "Exit")("threadNo", threadNo));
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
