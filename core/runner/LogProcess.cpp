// Copyright 2022 iLogtail Authors
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

#include "runner/LogProcess.h"

#include "app_config/AppConfig.h"
#include "pipeline/batch/TimeoutFlushManager.h"
#include "common/Flags.h"
#include "go_pipeline/LogtailPlugin.h"
#include "monitor/LogFileProfiler.h"
#include "monitor/LogtailAlarm.h"
#include "pipeline/PipelineManager.h"
#include "pipeline/queue/ExactlyOnceQueueManager.h"
#include "pipeline/queue/ProcessQueueManager.h"
#include "pipeline/queue/QueueKeyManager.h"

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

LogProcess::LogProcess() : mAccessProcessThreadRWL(ReadWriteLock::PREFER_WRITER) {
}

LogProcess::~LogProcess() {
    for (int32_t threadNo = 0; threadNo < mThreadCount; ++threadNo) {
        try {
            mProcessThreads[threadNo]->GetValue(1000 * 100);
        } catch (...) {
        }
    }
    delete[] mThreadFlags;
    delete[] mProcessThreads;
}

void LogProcess::Start() {
    if (mInitialized)
        return;
    mGlobalProcessQueueFullTotal = LoongCollectorMonitor::GetInstance()->GetIntGauge(METRIC_AGENT_PROCESS_QUEUE_FULL_TOTAL);
    mGlobalProcessQueueTotal = LoongCollectorMonitor::GetInstance()->GetIntGauge(METRIC_AGENT_PROCESS_QUEUE_TOTAL);

    mInitialized = true;
    mThreadCount = AppConfig::GetInstance()->GetProcessThreadCount();
    mProcessThreads = new ThreadPtr[mThreadCount];
    mThreadFlags = new atomic_bool[mThreadCount];
    for (int32_t threadNo = 0; threadNo < mThreadCount; ++threadNo) {
        mThreadFlags[threadNo] = false;
        mProcessThreads[threadNo] = CreateThread([this, threadNo]() { ProcessLoop(threadNo); });
    }
    LOG_INFO(sLogger, ("process daemon", "started"));
}

bool LogProcess::PushBuffer(QueueKey key, size_t inputIndex, PipelineEventGroup&& group, uint32_t retryTimes) {
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

void LogProcess::HoldOn() {
    LOG_INFO(sLogger, ("process daemon pause", "starts"));
    mAccessProcessThreadRWL.lock();
    while (true) {
        bool allThreadWait = true;
        for (int32_t threadNo = 0; threadNo < mThreadCount; ++threadNo) {
            if (mThreadFlags[threadNo]) {
                allThreadWait = false;
                break;
            }
        }
        if (allThreadWait) {
            LOG_INFO(sLogger, ("process daemon pause", "succeeded"));
            return;
        }
        usleep(10 * 1000);
    }
}

void LogProcess::Resume() {
    LOG_INFO(sLogger, ("process daemon resume", "starts"));
    mAccessProcessThreadRWL.unlock();
    LOG_INFO(sLogger, ("process daemon resume", "succeeded"));
}

bool LogProcess::FlushOut(int32_t waitMs) {
    ProcessQueueManager::GetInstance()->Trigger();
    if (ProcessQueueManager::GetInstance()->IsAllQueueEmpty()) {
        bool allThreadWait = true;
        for (int32_t threadNo = 0; threadNo < mThreadCount; ++threadNo) {
            if (mThreadFlags[threadNo]) {
                allThreadWait = false;
                break;
            } else {
                // sleep 1ms and double check
                usleep(1000);
                if (mThreadFlags[threadNo]) {
                    allThreadWait = false;
                    break;
                }
            }
        }
        if (allThreadWait)
            return true;
    }
    usleep(waitMs * 1000);
    return false;
}

void* LogProcess::ProcessLoop(int32_t threadNo) {
    LOG_DEBUG(sLogger, ("runner/LogProcess.hread", "Start")("threadNo", threadNo));
    static int32_t lastMergeTime = 0;
    static atomic_int s_processCount{0};
    static atomic_long s_processBytes{0};
    static atomic_int s_processLines{0};
    // only thread 0 update metric
    int32_t lastUpdateMetricTime = time(NULL);
    while (true) {
        mThreadFlags[threadNo] = false;

        int32_t curTime = time(NULL);
        if (threadNo == 0 && curTime - lastMergeTime >= INT32_FLAG(default_flush_merged_buffer_interval)) {
            TimeoutFlushManager::GetInstance()->FlushTimeoutBatch();
            lastMergeTime = curTime;
        }

        if (threadNo == 0 && curTime - lastUpdateMetricTime >= 40) {
            static auto sMonitor = LogtailMonitor::GetInstance();

            // atomic counter will be negative if process speed is too fast.
            sMonitor->UpdateMetric("process_tps", 1.0 * s_processCount / (curTime - lastUpdateMetricTime));
            sMonitor->UpdateMetric("process_bytes_ps", 1.0 * s_processBytes / (curTime - lastUpdateMetricTime));
            sMonitor->UpdateMetric("process_lines_ps", 1.0 * s_processLines / (curTime - lastUpdateMetricTime));
            lastUpdateMetricTime = curTime;
            s_processCount = 0;
            s_processBytes = 0;
            s_processLines = 0;

            // update process queue status
            uint32_t InvalidProcessQueueTotal = ProcessQueueManager::GetInstance()->GetInvalidCnt();
            sMonitor->UpdateMetric("process_queue_full", InvalidProcessQueueTotal);
            mGlobalProcessQueueFullTotal->Set(InvalidProcessQueueTotal);
            uint32_t ProcessQueueTotal = ProcessQueueManager::GetInstance()->GetCnt();
            sMonitor->UpdateMetric("process_queue_total", ProcessQueueTotal);
            mGlobalProcessQueueTotal->Set(ProcessQueueTotal);
            if (ExactlyOnceQueueManager::GetInstance()->GetProcessQueueCnt() > 0) {
                sMonitor->UpdateMetric("eo_process_queue_full",
                                       ExactlyOnceQueueManager::GetInstance()->GetInvalidProcessQueueCnt());
                sMonitor->UpdateMetric("eo_process_queue_total",
                                       ExactlyOnceQueueManager::GetInstance()->GetProcessQueueCnt());
            }
        }

        {
            ReadLock lock(mAccessProcessThreadRWL);

            unique_ptr<ProcessQueueItem> item;
            string configName;
            if (!ProcessQueueManager::GetInstance()->PopItem(threadNo, item, configName)) {
                ProcessQueueManager::GetInstance()->Wait(100);
                continue;
            }

            mThreadFlags[threadNo] = true;
            auto pipeline = PipelineManager::GetInstance()->FindConfigByName(configName);
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

            s_processCount++;
            if (isLog) {
                s_processBytes += profile.readBytes;
                s_processLines += profile.splitLines;
            }

            if (eventGroupList.empty()) {
                continue;
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
        }
    }
    LOG_WARNING(sLogger, ("runner/LogProcess.hread", "Exit")("threadNo", threadNo));
    return NULL;
}

bool LogProcess::Serialize(
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
