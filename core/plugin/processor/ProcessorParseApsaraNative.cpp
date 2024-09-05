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

#include "plugin/processor/ProcessorParseApsaraNative.h"

#include "app_config/AppConfig.h"
#include "common/LogtailCommonFlags.h"
#include "common/ParamExtractor.h"
#include "common/TimeUtil.h"
#include "models/LogEvent.h"
#include "monitor/MetricConstants.h"
#include "pipeline/plugin/instance/ProcessorInstance.h"

namespace logtail {

const std::string ProcessorParseApsaraNative::sName = "processor_parse_apsara_native";

const std::string SLS_KEY_LEVEL = "__LEVEL__";
const std::string SLS_KEY_THREAD = "__THREAD__";
const std::string SLS_KEY_FILE = "__FILE__";
const std::string SLS_KEY_LINE = "__LINE__";
const int32_t MAX_BASE_FIELD_NUM = 10;

bool ProcessorParseApsaraNative::Init(const Json::Value& config) {
    std::string errorMsg;

    // SourceKey
    if (!GetMandatoryStringParam(config, "SourceKey", mSourceKey, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    // Timezone
    if (!GetOptionalStringParam(config, "Timezone", mTimezone, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    } else if (!ParseLogTimeZoneOffsetSecond(mTimezone, mLogTimeZoneOffsetSecond)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             "string param Timezone is not valid",
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }

    if (!mCommonParserOptions.Init(config, *mContext, sName)) {
        return false;
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
    LogtailTime cachedLogTime = {0, 0};

    size_t wIdx = 0;
    for (size_t rIdx = 0; rIdx < events.size(); ++rIdx) {
        if (ProcessEvent(logPath, events[rIdx], cachedLogTime, timeStrCache)) {
            if (wIdx != rIdx) {
                events[wIdx] = std::move(events[rIdx]);
            }
            ++wIdx;
        }
    }
    events.resize(wIdx);
    return;
}

/*
 * 处理单个日志事件。
 * @param logPath - 日志文件的路径。
 * @param e - 指向待处理日志事件的智能指针。
 * @param cachedLogTime - 上一条日志的时间戳（秒）。
 * @param timeStrCache - 缓存时间字符串，用于比较和更新。
 * @return 如果事件被处理且保留，则返回true，如果事件被丢弃，则返回false。
 */
bool ProcessorParseApsaraNative::ProcessEvent(const StringView& logPath,
                                              PipelineEventPtr& e,
                                              LogtailTime& cachedLogTime,
                                              StringView& timeStrCache) {
    if (!IsSupportedEvent(e)) {
        return true;
    }
    LogEvent& sourceEvent = e.Cast<LogEvent>();
    if (!sourceEvent.HasContent(mSourceKey)) {
        return true;
    }
    bool sourceKeyOverwritten = false;
    StringView buffer = sourceEvent.GetContent(mSourceKey);
    if (buffer.size() == 0) {
        return true;
    }
    mProcParseInSizeBytes->Add(buffer.size());
    int64_t logTime_in_micro = 0;
    time_t logTime = ApsaraEasyReadLogTimeParser(buffer, timeStrCache, cachedLogTime, logTime_in_micro);
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
        sourceEvent.DelContent(mSourceKey);
        if (mCommonParserOptions.ShouldAddSourceContent(false)) {
            AddLog(mCommonParserOptions.mRenamedSourceKey, buffer, sourceEvent, false);
        }
        if (mCommonParserOptions.ShouldAddLegacyUnmatchedRawLog(false)) {
            AddLog(mCommonParserOptions.legacyUnmatchedRawLogKey, buffer, sourceEvent, false);
        }
        if (mCommonParserOptions.ShouldEraseEvent(false, sourceEvent)) {
            mProcDiscardRecordsTotal->Add(1);
            return false;
        }
        return true;
    }
    if (BOOL_FLAG(ilogtail_discard_old_data) && (time(NULL) - logTime) > INT32_FLAG(ilogtail_discard_interval)) {
        if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            StringView bufOut(buffer);
            if (buffer.size() > (size_t)(1024)) {
                bufOut = buffer.substr(0, 1024);
            }
            if (GetContext().GetAlarm().IsLowLevelAlarmValid()) {
                LOG_WARNING(
                    sLogger,
                    ("drop log event",
                     "log time falls more than " + ToString(INT32_FLAG(ilogtail_discard_interval))
                         + " secs behind current time")("log time", logTime)("gap", ToString(time(NULL) - logTime))(
                        "project", GetContext().GetProjectName())("logstore", GetContext().GetLogstoreName())(
                        "config", GetContext().GetConfigName())("file", logPath));
            }
            GetContext().GetAlarm().SendAlarm(OUTDATED_LOG_ALARM,
                                              std::string("logTime: ") + ToString(logTime)
                                                  + ", log:" + bufOut.to_string(),
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
    int32_t length = buffer.size();
    if (index < length) {
        for (index = index + 1; index <= length; ++index) {
            if (index == length || buffer.data()[index] == '\t') {
                if (colon_index >= 0) {
                    StringView key(buffer.data() + beg_index, colon_index - beg_index);
                    StringView data(buffer.data() + colon_index + 1, index - colon_index - 1);
                    AddLog(key, data, sourceEvent);
                    if (key == mSourceKey) {
                        sourceKeyOverwritten = true;
                    }
                    colon_index = -1;
                }
                beg_index = index + 1;
            } else if (buffer.data()[index] == ':' && colon_index == -1) {
                colon_index = index;
            }
        }
    }
    // logTime_in_micro = (int64_t)logTime_in_micro - (int64_t)mLogTimeZoneOffsetSecond * (int64_t)1000000;
    StringBuffer sb = sourceEvent.GetSourceBuffer()->AllocateStringBuffer(20);
#if defined(__linux__)
    sb.size = std::min(20, snprintf(sb.data, sb.capacity, "%ld", logTime_in_micro));
#elif defined(_MSC_VER)
    sb.size = std::min(20, snprintf(sb.data, sb.capacity, "%lld", logTime_in_micro));
#endif
    AddLog("microtime", StringView(sb.data, sb.size), sourceEvent);
    if (!sourceKeyOverwritten) {
        sourceEvent.DelContent(mSourceKey);
    }
    if (mCommonParserOptions.ShouldAddSourceContent(true)) {
        AddLog(mCommonParserOptions.mRenamedSourceKey, buffer, sourceEvent, false);
    }
    return true;
}

/*
 * 解析Apsara格式日志的时间。
 * @param buffer - 包含日志数据的字符串视图。
 * @param cachedTimeStr - 缓存的时间字符串。
 * @param cachedLogTime - 缓存的时间字符串的时间戳（秒），必须与cachedTimeStr同时修改。
 * @param microTime - 解析出的微秒时间戳。
 * @return 解析出的时间戳（秒），如果解析失败，则返回0。
 */
time_t ProcessorParseApsaraNative::ApsaraEasyReadLogTimeParser(StringView& buffer,
                                                               StringView& cachedTimeStr,
                                                               LogtailTime& cachedLogTime,
                                                               int64_t& microTime) {
    if (buffer[0] != '[') {
        return 0;
    }
    LogtailTime logTime = {};
    if (buffer[1] == '1') // for normal time, e.g 1378882630, starts with '1'
    {
        int nanosecondLength = 0;
        size_t pos = buffer.find(']', 1);
        if (pos == std::string::npos) {
            LOG_WARNING(sLogger, ("parse apsara log time", "fail")("string", buffer));
            return 0;
        }
        // strTime is the content between '[' and ']' and ends with '\0'
        std::string strTime = buffer.substr(1, pos).to_string();
        auto strptimeResult = Strptime(strTime.c_str(), "%s", &logTime, nanosecondLength);
        if (NULL == strptimeResult || strptimeResult[0] != ']') {
            LOG_WARNING(sLogger, ("parse apsara log time", "fail")("string", buffer)("timeformat", "%s"));
            return 0;
        }
        microTime = (int64_t)logTime.tv_sec * 1000000 + logTime.tv_nsec / 1000;
        return logTime.tv_sec;
    }
    // test other date format case
    {
        size_t pos = buffer.find(']', 1);
        if (pos == std::string::npos) {
            LOG_WARNING(sLogger, ("parse apsara log time", "fail")("string", buffer));
            return 0;
        }
        // strTime is the content between '[' and ']' and ends with '\0'
        std::string strTime = buffer.substr(1, pos).to_string();
        int nanosecondLength = 0;
        if (IsPrefixString(strTime, cachedTimeStr) == true) {
            if (strTime.size() > cachedTimeStr.size()) {
                auto strptimeResult
                    = Strptime(strTime.c_str() + cachedTimeStr.size() + 1, "%f", &logTime, nanosecondLength);
                if (NULL == strptimeResult) {
                    LOG_WARNING(sLogger,
                                ("parse apsara log time microsecond",
                                 "fail")("string", buffer)("timeformat", "%Y-%m-%d %H:%M:%S.%f"));
                }
            }
            microTime = (int64_t)cachedLogTime.tv_sec * 1000000 + logTime.tv_nsec / 1000;
            return cachedLogTime.tv_sec;
        }
        // parse second part
        auto strptimeResult = Strptime(strTime.c_str(), "%Y-%m-%d %H:%M:%S", &logTime, nanosecondLength);
        if (NULL == strptimeResult) {
            LOG_WARNING(sLogger,
                        ("parse apsara log time", "fail")("string", buffer)("timeformat", "%Y-%m-%d %H:%M:%S"));
            return 0;
        }
        // parse nanosecond part (optional)
        if (*strptimeResult != '\0') {
            strptimeResult = Strptime(strptimeResult + 1, "%f", &logTime, nanosecondLength);
            if (NULL == strptimeResult) {
                LOG_WARNING(sLogger,
                            ("parse apsara log time microsecond", "fail")("string", buffer)("timeformat",
                                                                                            "%Y-%m-%d %H:%M:%S.%f"));
            }
        }
        logTime.tv_sec = logTime.tv_sec - mLogTimeZoneOffsetSecond;
        microTime = (int64_t)logTime.tv_sec * 1000000 + logTime.tv_nsec / 1000;
        // if the time is valid (strptime not return NULL), the date value size must be 19 ,like '2013-09-11 03:11:05'
        cachedTimeStr = StringView(buffer.data() + 1, 19);
        cachedLogTime = logTime;
        return logTime.tv_sec;
    }
}

/*
 * 检查字符串是否包含指定的前缀。
 * @param all - 完整的字符串。
 * @param prefix - 要检查的前缀。
 * @return 如果字符串以指定前缀开头，则返回true；否则返回false。
 */
bool ProcessorParseApsaraNative::IsPrefixString(const std::string& all, const StringView& prefix) {
    return !prefix.empty() && std::equal(prefix.begin(), prefix.end(), all.begin());
}

/*
 * 查找Apsara格式日志的基础字段。
 * @param buffer - 包含日志数据的字符串视图。
 * @param beginIndexArray - 字段开始索引的数组。
 * @param endIndexArray - 字段结束索引的数组。
 * @return 解析到的基础字段数量。
 */
static int32_t FindBaseFields(const StringView& buffer, int32_t beginIndexArray[], int32_t endIndexArray[]) {
    int32_t baseFieldNum = 0;
    for (size_t i = 0; i < buffer.size(); i++) {
        if (buffer[i] == '[') {
            beginIndexArray[baseFieldNum] = i + 1;
        } else if (buffer[i] == ']') {
            if (i + 1 == buffer.size() || buffer[i + 1] == '\t' || buffer[i + 1] == '\n') {
                endIndexArray[baseFieldNum] = i;
                baseFieldNum++;
            }
            if (baseFieldNum >= MAX_BASE_FIELD_NUM) {
                break;
            }
            if (buffer[i + 1] == '\t' && (i + 2 == buffer.size() || buffer[i + 2] != '[')) {
                break;
            }
        }
    }
    return baseFieldNum;
}

/*
 * 检查是否为日志级别字段。
 * @param buffer - 包含日志数据的字符串视图。
 * @param beginIndex - 字段开始的索引。
 * @param endIndex - 字段结束的索引。
 * @return 如果字段是日志级别，则返回true；否则返回false。
 */
static bool IsFieldLevel(const StringView& buffer, int32_t beginIndex, int32_t endIndex) {
    for (int32_t i = beginIndex; i < endIndex; i++) {
        if (buffer[i] > 'Z' || buffer[i] < 'A') {
            return false;
        }
    }
    return true;
}

/*
 * 检查是否为线程ID字段。
 * @param buffer - 包含日志数据的字符串视图。
 * @param beginIndex - 字段开始的索引。
 * @param endIndex - 字段结束的索引。
 * @return 如果字段是线程ID，则返回true；否则返回false。
 */
static bool IsFieldThread(const StringView& buffer, int32_t beginIndex, int32_t endIndex) {
    for (int32_t i = beginIndex; i < endIndex; i++) {
        if (buffer[i] > '9' || buffer[i] < '0') {
            return false;
        }
    }
    return true;
}

/*
 * 检查是否为文件和行号字段。
 * @param buffer - 包含日志数据的字符串视图。
 * @param beginIndex - 字段开始的索引。
 * @param endIndex - 字段结束的索引。
 * @return 如果字段是文件和行号，则返回true；否则返回false。
 */
static bool IsFieldFileLine(const StringView& buffer, int32_t beginIndex, int32_t endIndex) {
    for (int32_t i = beginIndex; i < endIndex; i++) {
        if (buffer[i] == '/' || buffer[i] == '.') {
            return true;
        }
    }
    return false;
}

/*
 * 查找冒号字符的索引。
 * @param buffer - 包含日志数据的字符串视图。
 * @param beginIndex - 字段开始的索引。
 * @param endIndex - 字段结束的索引。
 * @return 冒号字符的索引，如果未找到，则返回endIndex。
 */
static int32_t FindColonIndex(const StringView& buffer, int32_t beginIndex, int32_t endIndex) {
    for (int32_t i = beginIndex; i < endIndex; i++) {
        if (buffer[i] == ':') {
            return i;
        }
    }
    return endIndex;
}

/*
 * 解析Apsara日志的基础字段并添加到日志事件中。
 * @param buffer - 包含日志数据的字符串视图。
 * @param sourceEvent - 引用到日志事件对象，用于添加解析出的字段。
 * @return 返回处理完基础字段后的索引位置。
 */
int32_t ProcessorParseApsaraNative::ParseApsaraBaseFields(const StringView& buffer, LogEvent& sourceEvent) {
    int32_t beginIndexArray[MAX_BASE_FIELD_NUM] = {0};
    int32_t endIndexArray[MAX_BASE_FIELD_NUM] = {0};
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
            AddLog(SLS_KEY_LEVEL, StringView(buffer.data() + beginIndex, endIndex - beginIndex), sourceEvent);
        } else if ((findFieldBitMap & 0x10) == 0 && IsFieldThread(buffer, beginIndex, endIndex)) {
            findFieldBitMap |= 0x10;
            AddLog(SLS_KEY_THREAD, StringView(buffer.data() + beginIndex, endIndex - beginIndex), sourceEvent);
        } else if ((findFieldBitMap & 0x100) == 0 && IsFieldFileLine(buffer, beginIndex, endIndex)) {
            findFieldBitMap |= 0x100;
            int32_t colonIndex = FindColonIndex(buffer, beginIndex, endIndex);
            AddLog(SLS_KEY_FILE, StringView(buffer.data() + beginIndex, colonIndex - beginIndex), sourceEvent);
            if (colonIndex < endIndex) {
                AddLog(
                    SLS_KEY_LINE, StringView(buffer.data() + colonIndex + 1, endIndex - colonIndex - 1), sourceEvent);
            }
        }
    }
    return endIndexArray[baseFieldNum - 1]; // return ']' position
}

void ProcessorParseApsaraNative::AddLog(const StringView& key,
                                        const StringView& value,
                                        LogEvent& targetEvent,
                                        bool overwritten) {
    if (!overwritten && targetEvent.HasContent(key)) {
        return;
    }
    targetEvent.AppendContentNoCopy(key, value);
    *mLogGroupSize += key.size() + value.size() + 5;
    mProcParseOutSizeBytes->Add(key.size() + value.size());
}

bool ProcessorParseApsaraNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<LogEvent>();
}

} // namespace logtail
