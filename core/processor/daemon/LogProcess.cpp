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

#include "LogProcess.h"

#include <sys/types.h>
#include <time.h>

#include <algorithm>
#if defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#endif
#include "aggregator/Aggregator.h"
#include "app_config/AppConfig.h"
#include "common/Constants.h"
#include "common/LogFileCollectOffsetIndicator.h"
#include "common/LogGroupContext.h"
#include "common/LogtailCommonFlags.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "config/IntegrityConfig.h"
#include "config_manager/ConfigManager.h"
#include "fuse/FuseFileBlacklist.h"
#include "go_pipeline/LogtailPlugin.h"
#include "log_pb/sls_logs.pb.h"
#include "logger/Logger.h"
#include "models/PipelineEventGroup.h"
#include "monitor/LogFileProfiler.h"
#include "monitor/LogIntegrity.h"
#include "monitor/LogLineCount.h"
#include "monitor/LogtailAlarm.h"
#include "monitor/Monitor.h"
#include "pipeline/PipelineManager.h"
#include "processor/inner/ProcessorParseContainerLogNative.h"
#include "sdk/Client.h"
#include "sender/Sender.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif


using namespace sls_logs;
using namespace std;

DEFINE_FLAG_INT32(process_buffer_count_upperlimit_perthread, "", 25);
DEFINE_FLAG_INT32(merge_send_packet_interval, "", 1);
DEFINE_FLAG_INT32(debug_logprocess_queue_flag, "0 disable, 1 true, 2 false", 0);
#if defined(_MSC_VER)
// On Windows, if Chinese config base path is used, the log path will be converted to GBK,
// so the __tag__.__path__ have to be converted back to UTF8 to avoid bad display.
// Note: enable this will spend CPU to do transformation.
DEFINE_FLAG_BOOL(enable_chinese_tag_path, "Enable Chinese __tag__.__path__", true);
#endif
DEFINE_FLAG_STRING(raw_log_tag, "", "__raw__");
DEFINE_FLAG_INT32(default_flush_merged_buffer_interval, "default flush merged buffer, seconds", 1);
DEFINE_FLAG_BOOL(enable_new_pipeline, "use C++ pipline with refactoried plugins", true);

namespace logtail {

LogProcess::LogProcess() : mAccessProcessThreadRWL(ReadWriteLock::PREFER_WRITER) {
    size_t concurrencyCount = (size_t)AppConfig::GetInstance()->GetSendRequestConcurrency();
    if (concurrencyCount < 20) {
        concurrencyCount = 20;
    }
    if (concurrencyCount > 50) {
        concurrencyCount = 50;
    }
    mLogFeedbackQueue.SetParam(concurrencyCount, (size_t)(concurrencyCount * 1.5), 100);
    mThreadCount = 0;
    mInitialized = false;
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

// Start() should only be called once except for UT
void LogProcess::Start() {
    if (mInitialized)
        return;
    mInitialized = true;
    // mLocalTimeZoneOffsetSecond = GetLocalTimeZoneOffsetSecond();
    // LOG_INFO(sLogger, ("local timezone offset second", mLocalTimeZoneOffsetSecond));
    Sender::Instance()->SetFeedBackInterface(&mLogFeedbackQueue);
    mThreadCount = AppConfig::GetInstance()->GetProcessThreadCount();
    // mBufferCountLimit = INT32_FLAG(process_buffer_count_upperlimit_perthread) * mThreadCount;
    mProcessThreads = new ThreadPtr[mThreadCount];
    mThreadFlags = new std::atomic_bool[mThreadCount];
    for (int32_t threadNo = 0; threadNo < mThreadCount; ++threadNo) {
        mThreadFlags[threadNo] = false;
        mProcessThreads[threadNo] = CreateThread([this, threadNo]() { ProcessLoop(threadNo); });
    }
    LOG_INFO(sLogger, ("process daemon", "started"));
}

bool LogProcess::PushBuffer(LogstoreFeedBackKey key,
                            const std::string& configName,
                            size_t inputIndex,
                            PipelineEventGroup&& group,
                            uint32_t retryTimes) {
    std::unique_ptr<ProcessQueueItem> item
        = std::unique_ptr<ProcessQueueItem>(new ProcessQueueItem(std::move(group), configName, inputIndex));
    for (size_t i = 0; i < retryTimes; ++i) {
        if (mLogFeedbackQueue.PushItem(key, std::move(item))) {
            return true;
        }
        if (i % 100 == 0) {
            LOG_WARNING(sLogger,
                        ("push attempts to process queue continuously failed for the past second",
                         "retry again")("config", configName)("input index", ToString(inputIndex)));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

bool LogProcess::IsValidToReadLog(const LogstoreFeedBackKey& logstoreKey) {
    if (INT32_FLAG(debug_logprocess_queue_flag) > 0) {
        return INT32_FLAG(debug_logprocess_queue_flag) == 1;
    }
    return mLogFeedbackQueue.IsValidToPush(logstoreKey);
}


void LogProcess::SetFeedBack(LogstoreFeedBackInterface* pInterface) {
    mLogFeedbackQueue.SetFeedBackObject(pInterface);
}

void LogProcess::SetPriorityWithHoldOn(const LogstoreFeedBackKey& logstoreKey, int32_t priority) {
    mLogFeedbackQueue.SetPriorityNoLock(logstoreKey, priority);
}

void LogProcess::DeletePriorityWithHoldOn(const LogstoreFeedBackKey& logstoreKey) {
    mLogFeedbackQueue.DeletePriorityNoLock(logstoreKey);
}

void LogProcess::HoldOn() {
    LOG_INFO(sLogger, ("process daemon pause", "starts"));
    mAccessProcessThreadRWL.lock();
    mLogFeedbackQueue.Lock();
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
    mLogFeedbackQueue.Unlock();
    mAccessProcessThreadRWL.unlock();
    LOG_INFO(sLogger, ("process daemon resume", "succeeded"));
}

bool LogProcess::FlushOut(int32_t waitMs) {
    mLogFeedbackQueue.Signal();
    if (mLogFeedbackQueue.IsEmpty()) {
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
    LOG_DEBUG(sLogger, ("LogProcessThread", "Start")("threadNo", threadNo));
    LogstoreFeedBackKey logstoreKey = 0;
    static int32_t lastMergeTime = 0;
    static atomic_int s_processCount{0};
    static atomic_long s_processBytes{0};
    static atomic_int s_processLines{0};
    // only thread 0 update metric
    int32_t lastUpdateMetricTime = time(NULL);
#ifdef LOGTAIL_DEBUG_FLAG
    int32_t lastPrintTime = time(NULL);
    uint64_t processCount = 0;
    uint64_t waitTime = 0;
    uint64_t waitCount = 0;
#endif
    while (true) {
        mThreadFlags[threadNo] = false;

        int32_t curTime = time(NULL);
        if (threadNo == 0 && curTime - lastMergeTime >= INT32_FLAG(default_flush_merged_buffer_interval)) {
            lastMergeTime = curTime;
            static Aggregator* aggregator = Aggregator::GetInstance();
            aggregator->FlushReadyBuffer();
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
            int32_t invalidCount = 0;
            int32_t totalCount = 0;
            int32_t eoInvalidCount = 0;
            int32_t eoTotalCount = 0;
            mLogFeedbackQueue.GetStatus(invalidCount, totalCount, eoInvalidCount, eoTotalCount);
            sMonitor->UpdateMetric("process_queue_full", invalidCount);
            sMonitor->UpdateMetric("process_queue_total", totalCount);
            if (eoTotalCount > 0) {
                sMonitor->UpdateMetric("eo_process_queue_full", eoInvalidCount);
                sMonitor->UpdateMetric("eo_process_queue_total", eoTotalCount);
            }
        }

        if (threadNo == 0) {
            LogIntegrity::GetInstance()->SendLogIntegrityInfo();
            LogLineCount::GetInstance()->SendLineCountData();

            DoFuseHandling();
        }

        // if have no data, wait 100 ms for new data or timeout, then continue to check again
        std::unique_ptr<ProcessQueueItem> item;
        if (!mLogFeedbackQueue.CheckAndPopNextItem(
                logstoreKey, item, Sender::Instance()->GetSenderFeedBackInterface(), threadNo, mThreadCount)) {
            mLogFeedbackQueue.Wait(100);
            continue;
        }
        {
            ReadLock lock(mAccessProcessThreadRWL);
            mThreadFlags[threadNo] = true;

            auto pipeline = PipelineManager::GetInstance()->FindPipelineByName(item->mConfigName);
            if (!pipeline) {
                LOG_INFO(sLogger,
                         ("pipeline not found during processing, perhaps due to config deletion",
                          "discard data")("config", item->mConfigName));
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
            std::vector<PipelineEventGroup> eventGroupList;
            eventGroupList.emplace_back(std::move(item->mEventGroup));
            pipeline->Process(eventGroupList);
            int32_t elapsedTime = (int32_t)time(NULL) - startTime;
            if (elapsedTime > 1) {
                LogtailAlarm::GetInstance()->SendAlarm(PROCESS_TOO_SLOW_ALARM,
                                                       string("event processing took too long, elapsed time: ")
                                                           + ToString(elapsedTime) + "s\tconfig: " + pipeline->Name(),
                                                       pipeline->GetContext().GetProjectName(),
                                                       pipeline->GetContext().GetLogstoreName(),
                                                       pipeline->GetContext().GetRegion());
                LOG_WARNING(sLogger,
                            ("event processing took too long, elapsed time",
                             ToString(elapsedTime) + "s")("config", pipeline->Name()));
            }

            s_processCount++;
            if (isLog) {
                s_processBytes += profile.readBytes;
                s_processLines += profile.splitLines;
            }

            // send part
            std::vector<std::unique_ptr<sls_logs::LogGroup>> logGroupList;
            bool needSend = ProcessBuffer(pipeline, eventGroupList, logGroupList);

            int logSize = 0;
            for (auto& pLogGroup : logGroupList) {
                logSize += pLogGroup->logs_size();
            }
            if (logSize > 0 && needSend) { // send log group
                const FlusherSLS* flusherSLS = static_cast<const FlusherSLS*>(pipeline->GetFlushers()[0]->GetPlugin());

                string compressStr = "zstd";
                if (flusherSLS->mCompressType == FlusherSLS::CompressType::NONE) {
                    compressStr = "none";
                } else if (flusherSLS->mCompressType == FlusherSLS::CompressType::LZ4) {
                    compressStr = "lz4";
                }
                sls_logs::SlsCompressType compressType = sdk::Client::GetCompressType(compressStr);

                const std::string& projectName = pipeline->GetContext().GetProjectName();
                const std::string& category = pipeline->GetContext().GetLogstoreName();
                string convertedPath = eventGroupList[0].GetMetadata(EventGroupMetaKey::LOG_FILE_PATH).to_string();
                string hostLogPath
                    = eventGroupList[0].GetMetadata(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED).to_string();
#if defined(_MSC_VER)
                if (BOOL_FLAG(enable_chinese_tag_path)) {
                    convertedPath = EncodingConverter::GetInstance()->FromACPToUTF8(convertedPath);
                    hostLogPath = EncodingConverter::GetInstance()->FromACPToUTF8(hostLogPath);
                }
#endif

                for (auto& pLogGroup : logGroupList) {
                    LogGroupContext context(flusherSLS->mRegion,
                                            projectName,
                                            flusherSLS->mLogstore,
                                            compressType,
                                            FileInfoPtr(),
                                            IntegrityConfigPtr(),
                                            LineCountConfigPtr(),
                                            -1,
                                            false,
                                            false,
                                            eventGroupList[0].GetExactlyOnceCheckpoint());
                    if (!Sender::Instance()->Send(
                            projectName,
                            eventGroupList[0].GetMetadata(EventGroupMetaKey::SOURCE_ID).to_string(),
                            *(pLogGroup.get()),
                            std::stol(eventGroupList[0].GetMetadata(EventGroupMetaKey::LOGGROUP_KEY).to_string()),
                            flusherSLS,
                            flusherSLS->mBatch.mMergeType,
                            (uint32_t)(profile.logGroupSize * DOUBLE_FLAG(loggroup_bytes_inflation)),
                            "",
                            convertedPath,
                            context)) {
                        LogtailAlarm::GetInstance()->SendAlarm(DISCARD_DATA_ALARM,
                                                               "push file data into batch map fail",
                                                               projectName,
                                                               category,
                                                               pipeline->GetContext().GetRegion());
                        LOG_ERROR(sLogger,
                                  ("push file data into batch map fail, discard logs", pLogGroup->logs_size())(
                                      "project", projectName)("logstore", category)("filename", convertedPath));
                    }
                }

                if (isLog) {
                    std::vector<sls_logs::LogTag> logTags;
                    for (auto& item : logGroupList[0]->logtags()) {
                        logTags.push_back(item);
                    }
                    LogFileProfiler::GetInstance()->AddProfilingData(
                        pipeline->Name(),
                        pipeline->GetContext().GetRegion(),
                        projectName,
                        category,
                        convertedPath,
                        hostLogPath,
                        logTags, // warning: this is not the same as reader extra tags!
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
            }
            logGroupList.clear();
        }
    }
    LOG_WARNING(sLogger, ("LogProcessThread", "Exit")("threadNo", threadNo));
    return NULL;
}

bool LogProcess::ProcessBuffer(const std::shared_ptr<Pipeline>& pipeline,
                               std::vector<PipelineEventGroup>& eventGroupList,
                               std::vector<std::unique_ptr<sls_logs::LogGroup>>& resultGroupList) {
    for (auto& eventGroup : eventGroupList) {
        // fill protobuf
        resultGroupList.emplace_back(new sls_logs::LogGroup());
        auto& resultGroup = resultGroupList.back();
        FillLogGroupLogs(eventGroup, *resultGroup, pipeline->GetContext().GetGlobalConfig().mEnableTimestampNanosecond);
        FillLogGroupTags(eventGroup, pipeline->GetContext().GetLogstoreName(), *resultGroup);
        if (pipeline->IsFlushingThroughGoPipeline()) {
            // LogGroup will be deleted outside
            LogtailPlugin::GetInstance()->ProcessLogGroup(
                pipeline->GetContext().GetConfigName(),
                *resultGroup,
                eventGroup.GetMetadata(EventGroupMetaKey::SOURCE_ID).to_string());
            return false;
        }
    }
    return true;
}

void LogProcess::FillLogGroupLogs(const PipelineEventGroup& eventGroup,
                                  sls_logs::LogGroup& resultGroup,
                                  bool enableTimestampNanosecond) const {
    for (auto& event : eventGroup.GetEvents()) {
        if (!event.Is<LogEvent>()) {
            continue;
        }
        sls_logs::Log* log = resultGroup.add_logs();
        auto& logEvent = event.Cast<LogEvent>();
        if (enableTimestampNanosecond) {
            SetLogTimeWithNano(log, logEvent.GetTimestamp(), logEvent.GetTimestampNanosecond());
        } else {
            SetLogTime(log, logEvent.GetTimestamp());
        }
        for (const auto& kv : logEvent) {
            sls_logs::Log_Content* contPtr = log->add_contents();
            // need to rename EVENT_META_LOG_FILE_OFFSET
            contPtr->set_key(kv.first.to_string());
            contPtr->set_value(kv.second.to_string());
        }
    }
}

void LogProcess::FillLogGroupTags(const PipelineEventGroup& eventGroup,
                                  const std::string& logstore,
                                  sls_logs::LogGroup& resultGroup) const {
    for (auto& tag : eventGroup.GetTags()) {
        auto logTagPtr = resultGroup.add_logtags();
        logTagPtr->set_key(tag.first.to_string());
        logTagPtr->set_value(tag.second.to_string());
    }

    if (resultGroup.category() != logstore) {
        resultGroup.set_category(logstore);
    }

    if (resultGroup.topic().empty()) {
        resultGroup.set_topic(eventGroup.GetMetadata(EventGroupMetaKey::TOPIC).to_string());
    }
    // source is set by sender
}

void LogProcess::DoFuseHandling() {
    LogFileCollectOffsetIndicator::GetInstance()->CalcFileOffset();
    LogFileCollectOffsetIndicator::GetInstance()->EliminateOutDatedItem();
    LogFileCollectOffsetIndicator::GetInstance()->ShrinkLogFileOffsetInfoMap();
}

#ifdef APSARA_UNIT_TEST_MAIN
void LogProcess::CleanEnviroments() {
    int32_t tryTime = 0;
    mLogFeedbackQueue.RemoveAll();
    while (true) {
        bool allThreadWait = true;
        for (int32_t threadNo = 0; threadNo < mThreadCount; ++threadNo) {
            if (mThreadFlags[threadNo]) {
                allThreadWait = false;
                break;
            }
        }
        if (allThreadWait)
            return;
        if (++tryTime % 100 == 0) {
            LOG_ERROR(sLogger, ("LogProcess CleanEnviroments is too slow or  blocked with unknow error.", ""));
        }
        usleep(10 * 1000);
    }
}
#endif

} // namespace logtail
