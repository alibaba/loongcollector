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

#include "processor/ProcessorParseApsaraNative.h"
#include "common/Constants.h"
#include "models/LogEvent.h"
#include "app_config/AppConfig.h"
#include "parser/LogParser.h" // for UNMATCH_LOG_KEY
#include "plugin/instance/ProcessorInstance.h"
#include "monitor/MetricConstants.h"
#include <algorithm>


namespace logtail {
const std::string ProcessorParseApsaraNative::sName = "processor_parse_apsara_native";

// static const int32_t MAX_BASE_FIELD_NUM = 10;

bool ProcessorParseApsaraNative::Init(const ComponentConfig& componentConfig) {
    const PipelineConfig& config = componentConfig.GetConfig();
    mSourceKey = DEFAULT_CONTENT_KEY;
    mDiscardUnmatch = config.mDiscardUnmatch;
    mUploadRawLog = config.mUploadRawLog;
    mRawLogTag = config.mAdvancedConfig.mRawLogTag;
    mLogTimeZoneOffsetSecond = config.mLogTimeZoneOffsetSecond;
    if (mUploadRawLog && mRawLogTag == mSourceKey) {
        mSourceKeyOverwritten = true;
    }
    mLogGroupSize = &(GetContext().GetProcessProfile().logGroupSize);
    mParseFailures = &(GetContext().GetProcessProfile().parseFailures);
    mHistoryFailures = &(GetContext().GetProcessProfile().historyFailures);

    mProcParseInSizeBytes = GetMetricsRecordRef().CreateCounter(METRIC_PROC_PARSE_IN_SIZE_BYTES);
    mProcParseOutSizeBytes = GetMetricsRecordRef().CreateCounter(METRIC_PROC_PARSE_OUT_SIZE_BYTES);
    mProcDiscardRecordsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PROC_DISCARD_RECORDS_TOTAL);
    mProcParseErrorTotal = GetMetricsRecordRef().CreateCounter(METRIC_PROC_PARSE_ERROR_TOTAL);
    mProcHistoryFailureTotal = GetMetricsRecordRef().CreateCounter(METRIC_PROC_HISTORY_FAILURE_TOTAL);
    return true;
}

void ProcessorParseApsaraNative::Process(PipelineEventGroup& logGroup) {
    if (logGroup.GetEvents().empty()) {
        return;
    }
    const StringView& logPath = logGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED);
    EventsContainer& events = logGroup.MutableEvents();
    StringView timeStrCache;
    LogtailTime lastLogTime;
    // works good normally. poor performance if most data need to be discarded.
    for (auto it = events.begin(); it != events.end();) {
        if (ProcessEvent(logPath, *it, lastLogTime, timeStrCache)) {
            ++it;
        } else {
            it = events.erase(it);
        }
    }
    return;
}

bool ProcessorParseApsaraNative::ProcessEvent(const StringView& logPath, PipelineEventPtr& e, LogtailTime& lastLogTime, StringView& timeStrCache) {
    if (!IsSupportedEvent(e)) {
        return true;
    }
    LogEvent& sourceEvent = e.Cast<LogEvent>();
    if (!sourceEvent.HasContent(mSourceKey)) {
        return true;
    }
    StringView buffer = sourceEvent.GetContent(mSourceKey);
    mProcParseInSizeBytes->Add(buffer.size());
    int64_t logTime_in_micro = 0;
    time_t logTime = ApsaraEasyReadLogTimeParser(buffer, timeStrCache, lastLogTime, logTime_in_micro);
    if (logTime <= 0) // this case will handle empty apsara log line
    {
        StringView bufOut(buffer);
        if (buffer.size() > (size_t)(1024)) {
            bufOut = buffer.substr(0, 1024);
        }
        if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            if (GetContext().GetAlarm().IsLowLevelAlarmValid()) {
                LOG_WARNING(sLogger,
                            ("discard error timeformat log", bufOut)("parsed time",
                                                                     logTime)("project", GetContext().GetProjectName())(
                                "logstore", GetContext().GetLogstoreName())("file", logPath));
            }
        }

        GetContext().GetAlarm().SendAlarm(PARSE_TIME_FAIL_ALARM,
                                               bufOut.to_string() + " $ " + ToString(logTime),
                                               GetContext().GetProjectName(),
                                               GetContext().GetLogstoreName(),
                                               GetContext().GetRegion());
        mProcParseErrorTotal->Add(1);
        ++(*mParseFailures);
        if (!mDiscardUnmatch) {
            AddLog(LogParser::UNMATCH_LOG_KEY, // __raw_log__
                   buffer,
                   sourceEvent); // legacy behavior, should use sourceKey
            if (mUploadRawLog) {
                AddLog(mRawLogTag, buffer, sourceEvent); // __raw__
            }
            return true;
        }
        mProcDiscardRecordsTotal->Add(1);
        return false;
    }
    if (BOOL_FLAG(ilogtail_discard_old_data)
        && (time(NULL) - logTime + mLogTimeZoneOffsetSecond) > INT32_FLAG(ilogtail_discard_interval)) {
        if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            StringView bufOut(buffer);
            if (buffer.size() > (size_t)(1024)) {
                bufOut = buffer.substr(0, 1024);
            }
            if (GetContext().GetAlarm().IsLowLevelAlarmValid()) {
                LOG_WARNING(sLogger,
                            ("discard history data, first 1k",
                             bufOut)("parsed time", logTime)("project", GetContext().GetProjectName())(
                                "logstore", GetContext().GetLogstoreName())("file", logPath));
            }
            GetContext().GetAlarm().SendAlarm(OUTDATED_LOG_ALARM,
                                              std::string("logTime: ") + ToString(logTime) + ", log:" + bufOut.to_string(),
                                              GetContext().GetProjectName(),
                                              GetContext().GetLogstoreName(),
                                              GetContext().GetRegion());
        }
        ++(*mHistoryFailures);
        mProcHistoryFailureTotal->Add(1);
        mProcDiscardRecordsTotal->Add(1);
        return false;
    }

    sourceEvent.SetTimestamp(logTime, logTime_in_micro * 1000 % 1000000000);
    int32_t beg_index = 0;
    int32_t colon_index = -1;
    int32_t index = -1;
    index = ParseApsaraBaseFields(buffer, sourceEvent);
    bool sourceKeyOverwritten = mSourceKeyOverwritten;
    bool rawLogTagOverwritten = false;
    if (buffer.data()[index] != 0) {
        do {
            ++index;
            if (buffer.data()[index] == '\t' || buffer.data()[index] == '\0') {
                if (colon_index >= 0) {
                    StringView key(buffer.data() + beg_index, colon_index - beg_index);
                    AddLog(key, StringView(buffer.data() + colon_index + 1, index - colon_index - 1), sourceEvent);
                    if (key == mSourceKey) {
                        sourceKeyOverwritten = true;
                    }
                    if (key == mRawLogTag) {
                        rawLogTagOverwritten = true;
                    }
                    colon_index = -1;
                }
                beg_index = index + 1;
            } else if (buffer.data()[index] == ':' && colon_index == -1) {
                colon_index = index;
            }
        } while (buffer.data()[index]);
    }
    // TODO: deprecated
    if (mAdjustApsaraMicroTimezone) {
        logTime_in_micro = (int64_t)logTime_in_micro - (int64_t)mLogTimeZoneOffsetSecond * (int64_t)1000000;
    }
    StringBuffer sb = sourceEvent.GetSourceBuffer()->AllocateStringBuffer(20);
#if defined(__linux__)
    sb.size = std::min(20, snprintf(sb.data, sb.capacity, "%ld", logTime_in_micro));
#elif defined(_MSC_VER)
    sb.size = std::min(20, snprintf(sb.data, sb.capacity, "%lld", logTime_in_micro));
#endif
    AddLog("microtime", StringView(sb.data, sb.size), sourceEvent);
    if (mUploadRawLog && !rawLogTagOverwritten) {
        AddLog(mRawLogTag, buffer, sourceEvent); // __raw__
    }
    if (!sourceKeyOverwritten) {
        sourceEvent.DelContent(mSourceKey);
    }
    return true;
}

time_t ProcessorParseApsaraNative::ApsaraEasyReadLogTimeParser(StringView& buffer, StringView& timeStr, LogtailTime& lastLogTime, int64_t& microTime) {
    if (buffer[0] != '[') {
        return 0;
    }
    if (buffer[1] == '1') // for normal time, e.g 1378882630, starts with '1'
    {
        int nanosecondLength = 0;
        auto strptimeResult = Strptime(buffer.data() + 1, "%s", &lastLogTime, nanosecondLength);
        if (NULL == strptimeResult || strptimeResult[0] != ']') {
            LOG_WARNING(sLogger,
                        ("parse apsara log time", "fail")("string", buffer)("timeformat", "%s"));
            return 0;
        }
        microTime = (int64_t)lastLogTime.tv_sec * 1000000 + lastLogTime.tv_nsec / 1000;
        return lastLogTime.tv_sec;
    }
    // test other date format case
    {
        if (IsPrefixString(buffer.data() + 1, timeStr) == true) {
            microTime = (int64_t)lastLogTime.tv_sec * 1000000 + lastLogTime.tv_nsec / 1000;
            return lastLogTime.tv_sec;
        }
        struct tm tm;
        memset(&tm, 0, sizeof(tm));
        int nanosecondLength = 0;
        auto strptimeResult = Strptime(buffer.data() + 1, "%Y-%m-%d %H:%M:%S.%f", &lastLogTime, nanosecondLength);
        if (NULL == strptimeResult || strptimeResult[0] != ']') {
            LOG_WARNING(sLogger,
                        ("parse apsara log time", "fail")("string", buffer)("timeformat", "%Y-%m-%d %H:%M:%S.%f"));
            return 0;
        }
        // if the time is valid (strptime not return NULL), the date value size must be 19 ,like '2013-09-11 03:11:05'
        timeStr = StringView(buffer.data() + 1, 19);
        microTime = (int64_t)lastLogTime.tv_sec * 1000000 + lastLogTime.tv_nsec / 1000;
        return lastLogTime.tv_sec;
    }
}

bool ProcessorParseApsaraNative::IsPrefixString(const char* all, const StringView& prefix) {
    if (prefix.size() == 0)
        return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (all[i] == '\0')
            return false;
        if (all[i] != prefix[i])
            return false;
    }
    return true;
}

static int32_t FindBaseFields(StringView& buffer, int32_t beginIndexArray[], int32_t endIndexArray[]) {
    int32_t baseFieldNum = 0;
    for (int32_t i = 0; buffer[i] != 0; i++) {
        if (buffer[i] == '[') {
            beginIndexArray[baseFieldNum] = i + 1;
        } else if (buffer[i] == ']') {
            if (buffer[i + 1] == '\t' || buffer[i + 1] == '\0' || buffer[i + 1] == '\n') {
                endIndexArray[baseFieldNum] = i;
                baseFieldNum++;
            }
            if (baseFieldNum >= LogParser::MAX_BASE_FIELD_NUM) {
                break;
            }
            if (buffer[i + 1] == '\t' && buffer[i + 2] != '[') {
                break;
            }
        }
    }
    return baseFieldNum;
}

static bool IsFieldLevel(StringView& buffer, int32_t beginIndex, int32_t endIndex) {
    for (int32_t i = beginIndex; i < endIndex; i++) {
        if (buffer[i] > 'Z' || buffer[i] < 'A') {
            return false;
        }
    }
    return true;
}

static bool IsFieldThread(StringView& buffer, int32_t beginIndex, int32_t endIndex) {
    for (int32_t i = beginIndex; i < endIndex; i++) {
        if (buffer[i] > '9' || buffer[i] < '0') {
            return false;
        }
    }
    return true;
}

static bool IsFieldFileLine(StringView& buffer, int32_t beginIndex, int32_t endIndex) {
    for (int32_t i = beginIndex; i < endIndex; i++) {
        if (buffer[i] == '/' || buffer[i] == '.') {
            return true;
        }
    }
    return false;
}

static int32_t FindColonIndex(StringView& buffer, int32_t beginIndex, int32_t endIndex) {
    for (int32_t i = beginIndex; i < endIndex; i++) {
        if (buffer[i] == ':') {
            return i;
        }
    }
    return endIndex;
}

int32_t ProcessorParseApsaraNative::ParseApsaraBaseFields(StringView& buffer, LogEvent& sourceEvent) {
    int32_t beginIndexArray[LogParser::MAX_BASE_FIELD_NUM] = {0};
    int32_t endIndexArray[LogParser::MAX_BASE_FIELD_NUM] = {0};
    int32_t baseFieldNum = FindBaseFields(buffer, beginIndexArray, endIndexArray);
    if (baseFieldNum == 0) {
        return 0;
    }
    int32_t beginIndex, endIndex;
    int32_t findFieldBitMap = 0x0;
    // i=0 field is the time field.
    for (int32_t i = 1; findFieldBitMap != 0x111 && i < baseFieldNum; i++) {
        beginIndex = beginIndexArray[i];
        endIndex = endIndexArray[i];
        if ((findFieldBitMap & 0x1) == 0 && IsFieldLevel(buffer, beginIndex, endIndex)) {
            findFieldBitMap |= 0x1;
            AddLog(LogParser::SLS_KEY_LEVEL, StringView(buffer.data() + beginIndex, endIndex - beginIndex), sourceEvent);
        } else if ((findFieldBitMap & 0x10) == 0 && IsFieldThread(buffer, beginIndex, endIndex)) {
            findFieldBitMap |= 0x10;
            AddLog(LogParser::SLS_KEY_THREAD, StringView(buffer.data() + beginIndex, endIndex - beginIndex), sourceEvent);
        } else if ((findFieldBitMap & 0x100) == 0 && IsFieldFileLine(buffer, beginIndex, endIndex)) {
            findFieldBitMap |= 0x100;
            int32_t colonIndex = FindColonIndex(buffer, beginIndex, endIndex);
            AddLog(LogParser::SLS_KEY_FILE, StringView(buffer.data() + beginIndex, endIndex - beginIndex), sourceEvent);
            if (colonIndex < endIndex) {
                AddLog(LogParser::SLS_KEY_LINE, StringView(buffer.data() + colonIndex + 1, endIndex - colonIndex - 1), sourceEvent);
            }
        }
    }
    return endIndexArray[baseFieldNum - 1]; // return ']' position
}

void ProcessorParseApsaraNative::AddLog(const StringView& key, const StringView& value, LogEvent& targetEvent) {
    targetEvent.SetContentNoCopy(key, value);
    *mLogGroupSize += key.size() + value.size() + 5;
    mProcParseOutSizeBytes->Add(key.size() + value.size());
}

bool ProcessorParseApsaraNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<LogEvent>();
}

} // namespace logtail