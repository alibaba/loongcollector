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

#include "plugin/processor/ProcessorParseTimestampNative.h"

#include "app_config/AppConfig.h"
#include "collection_pipeline/plugin/instance/ProcessorInstance.h"
#include "common/LogtailCommonFlags.h"
#include "common/ParamExtractor.h"
#include "monitor/metric_constants/MetricConstants.h"

namespace logtail {

const std::string ProcessorParseTimestampNative::sName = "processor_parse_timestamp_native";

bool ProcessorParseTimestampNative::Init(const Json::Value& config) {
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

    // SourceFormat
    if (!GetMandatoryStringParam(config, "SourceFormat", mSourceFormat, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    // SourceTimezone
    if (!GetOptionalStringParam(config, "SourceTimezone", mSourceTimezone, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    } else if (!ParseLogTimeZoneOffsetSecond(mSourceTimezone, mLogTimeZoneOffsetSecond)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             "string param SourceTimezone is not valid",
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }

    // SourceYear
    if (!GetOptionalIntParam(config, "SourceYear", mSourceYear, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mSourceYear,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    mDiscardedEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_DISCARDED_EVENTS_TOTAL);
    mOutFailedEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_FAILED_EVENTS_TOTAL);
    mOutKeyNotFoundEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_KEY_NOT_FOUND_EVENTS_TOTAL);
    mOutSuccessfulEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_SUCCESSFUL_EVENTS_TOTAL);
    mHistoryFailureTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_HISTORY_FAILURE_TOTAL);

    return true;
}

void ProcessorParseTimestampNative::Process(PipelineEventGroup& logGroup) {
    if (logGroup.GetEvents().empty() || mSourceFormat.empty() || mSourceKey.empty()) {
        return;
    }
    const StringView& logPath = logGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED);
    StringView timeStrCache;
    EventsContainer& events = logGroup.MutableEvents();
    LogtailTime logTime = {0, 0};

    size_t wIdx = 0;
    for (size_t rIdx = 0; rIdx < events.size(); ++rIdx) {
        if (ProcessEvent(logPath, events[rIdx], logTime, timeStrCache)) {
            if (wIdx != rIdx) {
                events[wIdx] = std::move(events[rIdx]);
            }
            ++wIdx;
        }
    }
    events.resize(wIdx);
    return;
}

bool ProcessorParseTimestampNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<LogEvent>();
}

bool ProcessorParseTimestampNative::ProcessEvent(StringView logPath,
                                                 PipelineEventPtr& e,
                                                 LogtailTime& logTime,
                                                 StringView& timeStrCache) {
    if (!IsSupportedEvent(e)) {
        ADD_COUNTER(mOutFailedEventsTotal, 1);
        return true;
    }
    LogEvent& sourceEvent = e.Cast<LogEvent>();
    if (!sourceEvent.HasContent(mSourceKey)) {
        ADD_COUNTER(mOutKeyNotFoundEventsTotal, 1);
        return true;
    }
    const StringView& timeStr = sourceEvent.GetContent(mSourceKey);
    uint64_t preciseTimestamp = 0;
    bool parseSuccess = ParseLogTime(timeStr, logPath, logTime, preciseTimestamp, timeStrCache);
    if (!parseSuccess) {
        ADD_COUNTER(mOutFailedEventsTotal, 1);
        return true;
    }
    if (logTime.tv_sec <= 0
        || (BOOL_FLAG(ilogtail_discard_old_data)
            && (time(NULL) - logTime.tv_sec) > INT32_FLAG(ilogtail_discard_interval))) {
        if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            if (AlarmManager::GetInstance()->IsLowLevelAlarmValid()) {
                LOG_WARNING(sLogger,
                            ("drop log event",
                             "log time falls more than " + ToString(INT32_FLAG(ilogtail_discard_interval))
                                 + " secs behind current time")("log time", logTime.tv_sec)(
                                "gap", ToString(time(NULL) - logTime.tv_sec))("project", GetContext().GetProjectName())(
                                "logstore", GetContext().GetLogstoreName())("config", GetContext().GetConfigName())(
                                "file", logPath));
            }
            AlarmManager::GetInstance()->SendAlarmWarning(OUTDATED_LOG_ALARM,
                                                          std::string("logTime: ") + ToString(logTime.tv_sec),
                                                          GetContext().GetRegion(),
                                                          GetContext().GetProjectName(),
                                                          GetContext().GetConfigName(),
                                                          GetContext().GetLogstoreName());
        }
        ADD_COUNTER(mHistoryFailureTotal, 1);
        ADD_COUNTER(mDiscardedEventsTotal, 1);
        return false;
    }
    sourceEvent.SetTimestamp(logTime.tv_sec, logTime.tv_nsec);
    // if (mLegacyPreciseTimestampConfig.enabled) {
    //     StringBuffer sb = sourceEvent.GetSourceBuffer()->AllocateStringBuffer(20);
    //     sb.size = std::min(20, snprintf(sb.data, sb.capacity, "%lu", preciseTimestamp));
    //     sourceEvent.SetContentNoCopy(mLegacyPreciseTimestampConfig.key, StringView(sb.data, sb.size));
    // }
    ADD_COUNTER(mOutSuccessfulEventsTotal, 1);
    return true;
}

bool ProcessorParseTimestampNative::ParseLogTime(const StringView& curTimeStr, // str to parse
                                                 const StringView& logPath,
                                                 LogtailTime& logTime,
                                                 uint64_t& preciseTimestamp,
                                                 StringView& timeStrCache // cache
) {
    // Second-level cache only work when:
    // 1. No %f in the time format
    // 2. The %f is at the end of the time format
    const char* compareResult = strstr(mSourceFormat.c_str(), "%f");
    bool haveNanosecond = compareResult != nullptr;
    bool endWithNanosecond = compareResult == (mSourceFormat.c_str() + mSourceFormat.size() - 2);
    int nanosecondLength = -1;
    const char* strptimeResult = NULL;
    if ((!haveNanosecond || endWithNanosecond) && IsPrefixString(curTimeStr, timeStrCache)) {
        bool isTimestampNanosecond = (mSourceFormat == "%s") && (curTimeStr.length() > timeStrCache.length());
        if (endWithNanosecond || isTimestampNanosecond) {
            strptimeResult = Strptime(curTimeStr.data() + timeStrCache.length(), "%f", &logTime, nanosecondLength);
        } else {
            strptimeResult = curTimeStr.data() + timeStrCache.length();
            logTime.tv_nsec = 0;
        }
    } else {
        strptimeResult = Strptime(curTimeStr.data(), mSourceFormat.c_str(), &logTime, nanosecondLength, mSourceYear);
        if (NULL != strptimeResult) {
            timeStrCache = curTimeStr.substr(0, curTimeStr.length() - nanosecondLength);
            logTime.tv_sec = logTime.tv_sec - mLogTimeZoneOffsetSecond;
        }
    }
    if (NULL == strptimeResult) {
        if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            if (AlarmManager::GetInstance()->IsLowLevelAlarmValid()) {
                LOG_WARNING(sLogger,
                            ("parse time fail", curTimeStr)("project", GetContext().GetProjectName())(
                                "logstore", GetContext().GetLogstoreName())("file", logPath));
            }
            AlarmManager::GetInstance()->SendAlarmWarning(PARSE_TIME_FAIL_ALARM,
                                                          curTimeStr.to_string() + " " + mSourceFormat,
                                                          GetContext().GetRegion(),
                                                          GetContext().GetProjectName(),
                                                          GetContext().GetConfigName(),
                                                          GetContext().GetLogstoreName());
        }
        return false;
    }

    // if (mLegacyPreciseTimestampConfig.enabled) {
    //     if (nanosecondLength < 0) {
    //         preciseTimestamp = GetPreciseTimestamp(logTime.tv_sec, strptimeResult, mLegacyPreciseTimestampConfig);
    //     } else {
    //         preciseTimestamp = GetPreciseTimestampFromLogtailTime(logTime, mLegacyPreciseTimestampConfig);
    //     }
    // }
    return true;
}

bool ProcessorParseTimestampNative::IsPrefixString(const StringView& all, const StringView& prefix) {
    if (all.size() < prefix.size())
        return false;
    if (prefix.size() == 0)
        return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (all[i] != prefix[i])
            return false;
    }
    return true;
}

} // namespace logtail
