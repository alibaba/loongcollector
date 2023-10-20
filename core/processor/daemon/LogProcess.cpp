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
#include <algorithm>
#include <time.h>
#include <sys/types.h>
#if defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#endif
#include "common/Constants.h"
#include "common/TimeUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/LogGroupContext.h"
#include "go_pipeline/LogtailPlugin.h"
#include "models/PipelineEventGroup.h"
#include "pipeline/PipelineManager.h"
#include "monitor/Monitor.h"
#include "parser/LogParser.h"
#include "sdk/Client.h"
#include "sender/Sender.h"
#include "log_pb/sls_logs.pb.h"
#include "common/StringTools.h"
#include "monitor/LogtailAlarm.h"
#include "monitor/LogIntegrity.h"
#include "monitor/LogLineCount.h"
#include "config/IntegrityConfig.h"
#include "app_config/AppConfig.h"
#include "monitor/LogFileProfiler.h"
#include "config_manager/ConfigManager.h"
#include "logger/Logger.h"
#include "aggregator/Aggregator.h"
#include "fuse/FuseFileBlacklist.h"
#include "common/LogFileCollectOffsetIndicator.h"


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
    mLocalTimeZoneOffsetSecond = GetLocalTimeZoneOffsetSecond();
    LOG_INFO(sLogger, ("local timezone offset second", mLocalTimeZoneOffsetSecond));
    Sender::Instance()->SetFeedBackInterface(&mLogFeedbackQueue);
    mThreadCount = AppConfig::GetInstance()->GetProcessThreadCount();
    // mBufferCountLimit = INT32_FLAG(process_buffer_count_upperlimit_perthread) * mThreadCount;
    mProcessThreads = new ThreadPtr[mThreadCount];
    mThreadFlags = new std::atomic_bool[mThreadCount];
    for (int32_t threadNo = 0; threadNo < mThreadCount; ++threadNo) {
        mThreadFlags[threadNo] = false;
        mProcessThreads[threadNo] = CreateThread([this, threadNo]() { ProcessLoop(threadNo); });
    }
}

bool LogProcess::PushBuffer(LogBuffer* buffer, int32_t retryTimes) {
    int32_t retry = 0;
    while (true) {
        retry++;
        if (!mLogFeedbackQueue.PushItem((buffer->logFileReader)->GetLogstoreKey(), buffer)) {
            if (retry % 100 == 0) {
                LOG_ERROR(sLogger,
                          ("Push log process buffer queue failed", ToString(buffer->rawBuffer.size()))(
                              buffer->logFileReader->GetProjectName(), buffer->logFileReader->GetCategory()));
            }
        } else {
            return true;
        }

        retry++;
        if (retry >= retryTimes)
            return false;
        usleep(10 * 1000);
    }
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
    mAccessProcessThreadRWL.lock();
    mLogFeedbackQueue.Lock();
    int32_t tryTime = 0;
    while (true) {
        bool allThreadWait = true;
        for (int32_t threadNo = 0; threadNo < mThreadCount; ++threadNo) {
            if (mThreadFlags[threadNo]) {
                allThreadWait = false;
                break;
            }
        }
        if (allThreadWait) {
            LOG_INFO(sLogger, ("LogProcess", "hold on"));
            return;
        }
        if (++tryTime % 100 == 0) {
            LOG_ERROR(sLogger, ("LogProcess thread is too slow or blocked with unknow error.", ""));
        }
        usleep(10 * 1000);
    }
}


void LogProcess::Resume() {
    mLogFeedbackQueue.Unlock();
    mAccessProcessThreadRWL.unlock();
    LOG_INFO(sLogger, ("LogProcess", "resume"));
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
        std::shared_ptr<LogBuffer> logBuffer;
        mThreadFlags[threadNo] = false;

        int32_t curTime = time(NULL);
        if (threadNo == 0 && curTime - lastMergeTime >= INT32_FLAG(default_flush_merged_buffer_interval)) {
            lastMergeTime = curTime;
            static Aggregator* aggregator = Aggregator::GetInstance();
            aggregator->FlushReadyBuffer();
        }

        if (threadNo == 0 && curTime - lastUpdateMetricTime >= 40) {
            static auto sMonitor = LogtailMonitor::Instance();

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
        LogBuffer* tmpLogBuffer = NULL;
        if (!mLogFeedbackQueue.CheckAndPopNextItem(
                logstoreKey, tmpLogBuffer, Sender::Instance()->GetSenderFeedBackInterface(), threadNo, mThreadCount)) {
            mLogFeedbackQueue.Wait(100);
            continue;
        }
        logBuffer.reset(tmpLogBuffer);

#ifdef LOGTAIL_DEBUG_FLAG
        ++processCount;
        if (curTime != lastPrintTime) {
            lastPrintTime = curTime;
            LOG_DEBUG(sLogger,
                      ("Logprocess tps", ToString(processCount))("wait time", ToString(waitTime))("wait count",
                                                                                                  ToString(waitCount)));
            processCount = 0;
            waitCount = 0;
            waitTime = 0;
        }
#endif
        {
            ReadLock lock(mAccessProcessThreadRWL);
            mThreadFlags[threadNo] = true;
            s_processCount++;
            uint64_t readBytes = logBuffer->rawBuffer.size() + 1; // may not be accurate if input is not utf8
            s_processBytes += readBytes;
            LogFileReaderPtr logFileReader = logBuffer->logFileReader;
            auto convertedPath = logFileReader->GetConvertedPath();
            auto hostLogPath = logFileReader->GetHostLogPath();
#if defined(_MSC_VER)
            if (BOOL_FLAG(enable_chinese_tag_path)) {
                convertedPath = EncodingConverter::GetInstance()->FromACPToUTF8(convertedPath);
                hostLogPath = EncodingConverter::GetInstance()->FromACPToUTF8(hostLogPath);
            }
#endif

            Config* config = ConfigManager::GetInstance()->FindConfigByName(logFileReader->GetConfigName());
            if (config == NULL) {
                LOG_INFO(sLogger,
                         ("can not find config while processing log, maybe config updated. config",
                          logFileReader->GetConfigName())("project", logFileReader->GetProjectName())(
                             "logstore", logFileReader->GetCategory()));
                continue;
            }

            sls_logs::LogGroup logGroup;
            ProcessProfile profile;
            profile.readBytes = readBytes;
            int32_t parseStartTime = (int32_t)time(NULL);
            bool needSend = false;
            if (!BOOL_FLAG(enable_new_pipeline)) {
                needSend = (0 == ProcessBufferLegacy(logBuffer, logFileReader, logGroup, profile, *config));
            } else {
                needSend = (0 == ProcessBuffer(logBuffer, logFileReader, logGroup, profile));
            }
            const std::string& projectName = config->GetProjectName();
            const std::string& category = config->GetCategory();
            int32_t parseEndTime = (int32_t)time(NULL);

            // add lines count
            s_processLines += profile.splitLines;
            // check whether processing is too slow
            if (parseEndTime - parseStartTime > 1) {
                LogtailAlarm::GetInstance()->SendAlarm(
                    PROCESS_TOO_SLOW_ALARM,
                    string("parse ") + ToString(logGroup.logs_size()) + " logs, buffer size "
                        + ToString(logBuffer->rawBuffer.size())
                        + "time used seconds : " + ToString(parseEndTime - parseStartTime),
                    projectName,
                    category,
                    config->mRegion);
                LOG_WARNING(sLogger,
                            ("process log too slow, parse logs", logGroup.logs_size())("buffer size",
                                                                                       logBuffer->rawBuffer.size())(
                                "time used seconds", parseEndTime - parseStartTime)("project", projectName)("logstore",
                                                                                                            category));
            }


            if (logGroup.logs_size() > 0 && needSend) { // send log group
                IntegrityConfig* integrityConfig = NULL;
                LineCountConfig* lineCountConfig = NULL;
                if (config->mIntegrityConfig->mIntegritySwitch) {
                    integrityConfig = new IntegrityConfig(config->mIntegrityConfig->mAliuid,
                                                          config->mIntegrityConfig->mIntegritySwitch,
                                                          config->mIntegrityConfig->mIntegrityProjectName,
                                                          config->mIntegrityConfig->mIntegrityLogstore,
                                                          config->mIntegrityConfig->mLogTimeReg,
                                                          config->mIntegrityConfig->mTimeFormat,
                                                          config->mIntegrityConfig->mTimePos);
                }
                if (config->mLineCountConfig->mLineCountSwitch) {
                    lineCountConfig = new LineCountConfig(config->mLineCountConfig->mAliuid,
                                                          config->mLineCountConfig->mLineCountSwitch,
                                                          config->mLineCountConfig->mLineCountProjectName,
                                                          config->mLineCountConfig->mLineCountLogstore);
                }
                IntegrityConfigPtr integrityConfigPtr(integrityConfig);
                LineCountConfigPtr lineCountConfigPtr(lineCountConfig);
                sls_logs::SlsCompressType compressType = sdk::Client::GetCompressType(config->mCompressType);

                LogGroupContext context(config->mRegion,
                                        projectName,
                                        config->mCategory,
                                        compressType,
                                        logBuffer->fileInfo,
                                        integrityConfigPtr,
                                        lineCountConfigPtr,
                                        -1,
                                        logFileReader->GetFuseMode(),
                                        logFileReader->GetMarkOffsetFlag(),
                                        logBuffer->exactlyOnceCheckpoint);
                if (!Sender::Instance()->Send(projectName,
                                              logFileReader->GetSourceId(),
                                              logGroup,
                                              config,
                                              config->mMergeType,
                                              (uint32_t)(profile.logGroupSize * DOUBLE_FLAG(loggroup_bytes_inflation)),
                                              "",
                                              convertedPath,
                                              context)) {
                    LogtailAlarm::GetInstance()->SendAlarm(DISCARD_DATA_ALARM,
                                                           "push file data into batch map fail",
                                                           projectName,
                                                           category,
                                                           config->mRegion);
                    LOG_ERROR(sLogger,
                              ("push file data into batch map fail, discard logs", logGroup.logs_size())(
                                  "project", projectName)("logstore", category)("filename", convertedPath));
                }
            }

            LogFileProfiler::GetInstance()->AddProfilingData(config->mConfigName,
                                                             config->mRegion,
                                                             projectName,
                                                             category,
                                                             convertedPath,
                                                             hostLogPath,
                                                             logFileReader->GetExtraTags(),
                                                             readBytes,
                                                             profile.skipBytes,
                                                             profile.splitLines,
                                                             profile.parseFailures,
                                                             profile.regexMatchFailures,
                                                             profile.parseTimeFailures,
                                                             profile.historyFailures,
                                                             0,
                                                             ""); // TODO: I don't think errorLine is useful
            LOG_DEBUG(
                sLogger,
                ("project", projectName)("logstore", category)("filename", convertedPath)("read_bytes", readBytes)(
                    "line_feed", profile.feedLines)("split_lines", profile.splitLines)(
                    "parse_failures", profile.parseFailures)("parse_time_failures", profile.parseTimeFailures)(
                    "regex_match_failures", profile.regexMatchFailures)("history_failures", profile.historyFailures));
        }
    }
    LOG_WARNING(sLogger, ("LogProcessThread", "Exit")("threadNo", threadNo));
    return NULL;
}

int LogProcess::ProcessBuffer(std::shared_ptr<LogBuffer>& logBuffer,
                              LogFileReaderPtr& logFileReader,
                              sls_logs::LogGroup& resultGroup,
                              ProcessProfile& profile) {
    auto pipeline = PipelineManager::GetInstance()->FindPipelineByName(
        logFileReader->GetConfigName()); // pipeline should be set in the loggroup by input
    if (pipeline.get() == nullptr) {
        LOG_INFO(sLogger,
                 ("can not find pipeline while processing log, maybe config deleted. config",
                  logFileReader->GetConfigName())("project", logFileReader->GetProjectName())(
                     "logstore", logFileReader->GetCategory()));
        return -1;
    }

    std::vector<PipelineEventGroup> outputList;
    {
        // construct a logGroup, it should be moved into input later
        PipelineEventGroup eventGroup(logBuffer);
        // TODO: metadata should be set in reader
        FillEventGroupMetadata(*logBuffer, eventGroup);

        std::unique_ptr<LogEvent> event = LogEvent::CreateEvent(eventGroup.GetSourceBuffer());
        time_t logtime = time(NULL);
        if (AppConfig::GetInstance()->EnableLogTimeAutoAdjust()) {
            logtime += GetTimeDelta();
        }
        event->SetTimestamp(logtime);
        event->SetContentNoCopy(DEFAULT_CONTENT_KEY, logBuffer->rawBuffer);
        auto offsetStr = event->GetSourceBuffer()->CopyString(std::to_string(logBuffer->readOffset));
        event->SetContentNoCopy(LOG_RESERVED_KEY_FILE_OFFSET, StringView(offsetStr.data, offsetStr.size));
        eventGroup.AddEvent(std::move(event));
        // process logGroup
        pipeline->Process(std::move(eventGroup), outputList);
    }

    // record profile
    auto& processProfile = pipeline->GetContext().GetProcessProfile();
    profile = processProfile;
    processProfile.Reset();

    for (auto& eventGroup : outputList) {
        // fill protobuf
        FillLogGroupLogs(eventGroup, resultGroup, pipeline->GetPipelineConfig().mAdvancedConfig.mEnableTimestampNanosecond);
        FillLogGroupTags(eventGroup, logFileReader, resultGroup);
        if (logFileReader->GetPluginFlag()) {
            LogtailPlugin::GetInstance()->ProcessLogGroup(
                logFileReader->GetConfigName(), resultGroup, logFileReader->GetSourceId());
            return 1;
        }
        // record log positions for exactly once. TODO: make it correct for each log, current implementation requires
        // loggroup send in one shot
        if (logBuffer->exactlyOnceCheckpoint) {
            std::pair<size_t, size_t> pos(logBuffer->readOffset, logBuffer->readLength);
            logBuffer->exactlyOnceCheckpoint->positions.assign(eventGroup.GetEvents().size(), pos);
        }
    }
    return 0;
}

void LogProcess::FillEventGroupMetadata(LogBuffer& logBuffer, PipelineEventGroup& eventGroup) const {
    eventGroup.SetMetadataNoCopy(EventGroupMetaKey::LOG_FILE_PATH, logBuffer.logFileReader->GetConvertedPath());
    eventGroup.SetMetadataNoCopy(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED, logBuffer.logFileReader->GetHostLogPath());
    eventGroup.SetMetadata(EventGroupMetaKey::LOG_FILE_INODE,
                           std::to_string(logBuffer.logFileReader->GetDevInode().inode));
    std::string agentTag = ConfigManager::GetInstance()->GetUserDefinedIdSet();
    if (!agentTag.empty()) {
        eventGroup.SetMetadata(EventGroupMetaKey::AGENT_TAG, ConfigManager::GetInstance()->GetUserDefinedIdSet());
    }
    eventGroup.SetMetadataNoCopy(EventGroupMetaKey::HOST_IP, LogFileProfiler::mIpAddr);
    eventGroup.SetMetadataNoCopy(EventGroupMetaKey::HOST_NAME, LogFileProfiler::mHostname);
    eventGroup.SetMetadata(EventGroupMetaKey::LOG_READ_OFFSET, std::to_string(logBuffer.readOffset));
    eventGroup.SetMetadata(EventGroupMetaKey::LOG_READ_OFFSET, std::to_string(logBuffer.readLength));
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
        for (auto& kv : logEvent.GetContents()) {
            sls_logs::Log_Content* contPtr = log->add_contents();
            // need to rename EVENT_META_LOG_FILE_OFFSET
            contPtr->set_key(kv.first.to_string());
            contPtr->set_value(kv.second.to_string());
        }
    }
}

void LogProcess::FillLogGroupTags(const PipelineEventGroup& eventGroup,
                                  LogFileReaderPtr& logFileReader,
                                  sls_logs::LogGroup& resultGroup) const {
    // fill tags from eventGroup
    for (auto& tag : eventGroup.GetTags()) {
        auto logTagPtr = resultGroup.add_logtags();
        logTagPtr->set_key(tag.first.to_string());
        logTagPtr->set_value(tag.second.to_string());
    }

    // special tags from reader
    const std::vector<sls_logs::LogTag>& extraTags = logFileReader->GetExtraTags();
    for (size_t i = 0; i < extraTags.size(); ++i) {
        auto logTagPtr = resultGroup.add_logtags();
        logTagPtr->set_key(extraTags[i].key());
        logTagPtr->set_value(extraTags[i].value());
    }

    if (resultGroup.category() != logFileReader->GetCategory()) {
        resultGroup.set_category(logFileReader->GetCategory());
    }

    if (resultGroup.topic().empty()) {
        resultGroup.set_topic(logFileReader->GetTopicName());
    }
}

int LogProcess::ProcessBufferLegacy(std::shared_ptr<LogBuffer>& logBuffer,
                                    LogFileReaderPtr& logFileReader,
                                    sls_logs::LogGroup& logGroup,
                                    ProcessProfile& profile,
                                    Config& config) {
    auto logPath = logFileReader->GetConvertedPath();
    // Mixed mode, pass buffer to plugin system.
    if (logFileReader->GetPluginFlag()) {
        if (!config.PassingTagsToPlugin()) // V1
        {
            LogtailPlugin::GetInstance()->ProcessRawLog(logFileReader->GetConfigName(),
                                                        logBuffer->rawBuffer,
                                                        logFileReader->GetSourceId(),
                                                        logFileReader->GetTopicName());
        } else // V2
        {
            static const std::string TAG_DELIMITER = "^^^";
            static const std::string TAG_SEPARATOR = "~=~";
            static const std::string TAG_PREFIX = "__tag__:";

            // Collect tags to pass, __hostname__ will be added in plugin.
            std::string passingTags;
            passingTags.append(TAG_PREFIX)
                .append(LOG_RESERVED_KEY_PATH)
                .append(TAG_SEPARATOR)
                .append(logPath.substr(0, 511));

            std::string userDefinedId = ConfigManager::GetInstance()->GetUserDefinedIdSet();
            if (!userDefinedId.empty()) {
                passingTags.append(TAG_DELIMITER)
                    .append(TAG_PREFIX)
                    .append(LOG_RESERVED_KEY_USER_DEFINED_ID)
                    .append(TAG_SEPARATOR)
                    .append(userDefinedId.substr(0, 99));
            }
            const std::vector<sls_logs::LogTag>& extraTags = logFileReader->GetExtraTags();
            for (size_t i = 0; i < extraTags.size(); ++i) {
                passingTags.append(TAG_DELIMITER)
                    .append(TAG_PREFIX)
                    .append(extraTags[i].key())
                    .append(TAG_SEPARATOR)
                    .append(extraTags[i].value());
            }

            if (config.mAdvancedConfig.mEnableLogPositionMeta) {
                passingTags.append(TAG_DELIMITER)
                    .append(TAG_PREFIX)
                    .append(LOG_RESERVED_KEY_FILE_OFFSET)
                    .append(TAG_SEPARATOR)
                    .append(std::to_string(logBuffer->readOffset));
            }

            LogtailPlugin::GetInstance()->ProcessRawLogV2(logFileReader->GetConfigName(),
                                                          logBuffer->rawBuffer,
                                                          logFileReader->GetSourceId(),
                                                          logFileReader->GetTopicName(),
                                                          passingTags);
        }
        return 1;
    }

    StringView& rawBuffer = logBuffer->rawBuffer;
    int32_t lineFeed = 0;
    std::vector<StringView> logIndex; // all splitted logs
    std::vector<StringView> discardIndex; // used to send warning
    bool splitSuccess = logFileReader->LogSplit(rawBuffer.data(), rawBuffer.size(), lineFeed, logIndex, discardIndex);

    const std::string& projectName = config.GetProjectName();
    const std::string& category = config.GetCategory();
    ParseLogError error;
    uint32_t lines = logIndex.size();
    //////////////////////////////////////////////
    // for profiling
    profile.splitLines = lines;
    string errorLine;
    //////////////////////////////////////////////

    if (AppConfig::GetInstance()->IsLogParseAlarmValid() && LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
        if (!splitSuccess) { // warning if unsplittable
            LogtailAlarm::GetInstance()->SendAlarm(SPLIT_LOG_FAIL_ALARM,
                                                   "split log lines fail, please check log_begin_regex, file:" + logPath
                                                       + ", logs:" + rawBuffer.substr(0, 1024).to_string(),
                                                   projectName,
                                                   category,
                                                   config.mRegion);
            LOG_ERROR(sLogger,
                      ("split log lines fail", "please check log_begin_regex")("file_name", logPath)(
                          "read bytes", profile.readBytes)("first 1KB log", rawBuffer.substr(0, 1024).to_string()));
        }
        for (auto& discardData : discardIndex) { // warning if data loss
            LogtailAlarm::GetInstance()->SendAlarm(SPLIT_LOG_FAIL_ALARM,
                                                   "split log lines discard data, file:" + logPath
                                                       + ", logs:" + discardData.substr(0, 1024).to_string(),
                                                   projectName,
                                                   category,
                                                   config.mRegion);
            LOG_WARNING(sLogger,
                        ("split log lines discard data", "please check log_begin_regex")("file_name", logPath)(
                            "read bytes", profile.readBytes)("first 1KB log", discardData.substr(0, 1024).to_string()));
        }
    }

    if (lines > 0) {
        // @debug
        // static int linesCount = 0;
        // linesCount += lines;
        // LOG_INFO(sLogger, ("Logprocess lines", lines)("Total lines", linesCount));
        LogtailTime lastLogLineTime = {0, 0};
        string lastLogTimeStr = "";
        uint32_t logGroupSize = 0;
        int32_t successLogSize = 0;
        for (uint32_t i = 0; i < lines; i++) {
            bool successful = logFileReader->ParseLogLine(
                logIndex[i], logGroup, error, lastLogLineTime, lastLogTimeStr, logGroupSize);
            if (!successful) {
                ++profile.parseFailures;
                if (error == PARSE_LOG_REGEX_ERROR)
                    ++profile.regexMatchFailures;
                else if (error == PARSE_LOG_TIMEFORMAT_ERROR)
                    ++profile.parseTimeFailures;
                else if (error == PARSE_LOG_HISTORY_ERROR)
                    ++profile.historyFailures;
                if (errorLine.empty())
                    errorLine = logIndex[i].to_string();
            }
            // add source raw line, time zone adjust
            if (successLogSize < logGroup.logs_size()) {
                sls_logs::Log* logPtr = logGroup.mutable_logs(successLogSize);
                if (logPtr != NULL) {
                    if (!config.mAdvancedConfig.mEnableTimestampNanosecond) {
                        logPtr->clear_time_ns();
                    }
                    if (config.mUploadRawLog) {
                        LogParser::AddLog(
                            logPtr, config.mAdvancedConfig.mRawLogTag, logIndex[i].to_string(), logGroupSize);
                    }
                    if (AppConfig::GetInstance()->EnableLogTimeAutoAdjust()) {
                        logPtr->set_time(logPtr->time() + GetTimeDelta());
                    }
                }
                successLogSize = logGroup.logs_size();

                // record offset in content
                // TODO: I don't think all offsets calc below works with GBK
                auto const offset = logBuffer->readOffset + (logIndex[i].data() - rawBuffer.data());
                if ((logBuffer->exactlyOnceCheckpoint || config.mAdvancedConfig.mEnableLogPositionMeta)
                    && logPtr != nullptr) {
                    auto content = logPtr->add_contents();
                    content->set_key(LOG_RESERVED_KEY_FILE_OFFSET);
                    content->set_value(std::to_string(offset));
                }
                // record log positions for exactly once.
                if (logBuffer->exactlyOnceCheckpoint && logPtr != nullptr) {
                    int32_t length = 0;
                    if (1 == lines) {
                        length = logBuffer->readLength;
                    } else if (i != lines - 1) {
                        length = logIndex[i + 1].data() - logIndex[i].data();
                    } else {
                        length = logBuffer->readLength - (logIndex[i].data() - rawBuffer.data());
                    }
                    logBuffer->exactlyOnceCheckpoint->positions.emplace_back(
                        std::make_pair(offset, static_cast<size_t>(length)));
                }
            }
        }

        // add common predefined tags
        if (logGroup.logs_size() > 0) {
            sls_logs::LogTag* logTagPtr = logGroup.add_logtags();
            logTagPtr->set_key(LOG_RESERVED_KEY_HOSTNAME);
            logTagPtr->set_value(LogFileProfiler::mHostname.substr(0, 99));
            logTagPtr = logGroup.add_logtags();
            logTagPtr->set_key(LOG_RESERVED_KEY_PATH);
            logTagPtr->set_value(logPath.substr(0, 511));

            // zone info for ant
            const std::string& alipayZone = AppConfig::GetInstance()->GetAlipayZone();
            if (!alipayZone.empty()) {
                logTagPtr = logGroup.add_logtags();
                logTagPtr->set_key(LOG_RESERVED_KEY_ALIPAY_ZONE);
                logTagPtr->set_value(alipayZone);
            }

            string userDefinedId = ConfigManager::GetInstance()->GetUserDefinedIdSet();
            if (userDefinedId.size() > 0) {
                logTagPtr = logGroup.add_logtags();
                logTagPtr->set_key(LOG_RESERVED_KEY_USER_DEFINED_ID);
                logTagPtr->set_value(userDefinedId.substr(0, 99));
            }

            const std::vector<sls_logs::LogTag>& extraTags = logFileReader->GetExtraTags();
            for (size_t i = 0; i < extraTags.size(); ++i) {
                logTagPtr = logGroup.add_logtags();
                logTagPtr->set_key(extraTags[i].key());
                logTagPtr->set_value(extraTags[i].value());
            }

            // add truncate info to loggroup
            if (config.mIsFuseMode && logBuffer->truncateInfo.get() != NULL
                && logBuffer->truncateInfo->empty() == false) {
                sls_logs::LogTag* logTagPtr = logGroup.add_logtags();
                logTagPtr->set_key(LOG_RESERVED_KEY_TRUNCATE_INFO);
                logTagPtr->set_value(logBuffer->truncateInfo->toString());
            }

            if (logGroup.category() != category) {
                logGroup.set_category(category);
            }

            if (logGroup.topic().empty()) {
                logGroup.set_topic(logFileReader->GetTopicName());
            }
        }
    }
    return 0;
}

void LogProcess::DoFuseHandling() {
    LogFileCollectOffsetIndicator::GetInstance()->CalcFileOffset();
    LogFileCollectOffsetIndicator::GetInstance()->EliminateOutDatedItem();
    LogFileCollectOffsetIndicator::GetInstance()->ShrinkLogFileOffsetInfoMap();

    if (ConfigManager::GetInstance()->HaveFuseConfig()) {
        FuseFileBlacklist::GetInstance()->RemoveFile();
    }
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
