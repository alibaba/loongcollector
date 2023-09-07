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

#include "processor/ProcessorParseTimestampNative.h"

#include "app_config/AppConfig.h"
#include "common/Constants.h"
#include "common/LogtailCommonFlags.h"
#include "plugin/ProcessorInstance.h"


namespace logtail {
bool ProcessorParseTimestampNative::Init(const ComponentConfig& componentConfig) {
    PipelineConfig config = componentConfig.GetConfig();
    mTimeFormat = config.mTimeFormat;
    mTimeKey = config.mTimeKey;
    mSpecifiedYear = config.mAdvancedConfig.mSpecifiedYear;
    mLegacyPreciseTimestampConfig.enabled = config.mAdvancedConfig.mEnablePreciseTimestamp;
    mLegacyPreciseTimestampConfig.key = config.mAdvancedConfig.mPreciseTimestampKey;
    mLegacyPreciseTimestampConfig.unit = config.mAdvancedConfig.mPreciseTimestampUnit;
    mLogTimeZoneOffsetSecond = config.mLogTimeZoneOffsetSecond;

    mParseTimeFailures = &(GetContext().GetProcessProfile().parseTimeFailures);
    mHistoryFailures = &(GetContext().GetProcessProfile().historyFailures);
    SetMetricsRecordRef(Name(), componentConfig.GetId());
    return true;
}

void ProcessorParseTimestampNative::Process(PipelineEventGroup& logGroup) {
    if (logGroup.GetEvents().empty() || mTimeFormat.empty() || mTimeKey.empty()) {
        return;
    }
    const StringView& logPath = logGroup.GetMetadata(EVENT_META_LOG_FILE_PATH_RESOLVED);
    StringView timeStrCache;
    EventsContainer& events = logGroup.MutableEvents();
    // works good normally. poor performance if most data need to be discarded.
    for (auto it = events.begin(); it != events.end();) {
        if (ProcessorParseTimestampNative::ProcessEvent(logPath, *it, timeStrCache)) {
            ++it;
        } else {
            it = events.erase(it);
        }
    }
    return;
}

bool ProcessorParseTimestampNative::IsSupportedEvent(const PipelineEventPtr& e) {
    return e.Is<LogEvent>();
}

bool ProcessorParseTimestampNative::ProcessEvent(StringView logPath, PipelineEventPtr& e, StringView& timeStrCache) {
    if (!IsSupportedEvent(e)) {
        return true;
    }
    LogEvent& sourceEvent = e.Cast<LogEvent>();
    if (!sourceEvent.HasContent(mTimeKey)) {
        return true;
    }
    const StringView& timeStr = sourceEvent.GetContent(mTimeKey);
    LogtailTime logTime = {0, 0};
    uint64_t preciseTimestamp = 0;
    bool parseSuccess = ParseLogTime(timeStr, logPath, logTime, preciseTimestamp, timeStrCache);
    if (!parseSuccess) {
        return true;
    }
    if (logTime.tv_sec <= 0
        || (BOOL_FLAG(ilogtail_discard_old_data)
            // Adjust time(NULL) from local timezone to target timezone
            && (time(NULL) - logTime.tv_sec) > INT32_FLAG(ilogtail_discard_interval))) {
        if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                LOG_WARNING(sLogger,
                            ("discard history data", timeStr)("time now", time(NULL))("timestamp", logTime.tv_sec)(
                                "nanosecond", logTime.tv_nsec)("tzOffsetSecond", mLogTimeZoneOffsetSecond)(
                                "INT32_FLAG(ilogtail_discard_interval)",
                                INT32_FLAG(ilogtail_discard_interval))("project", GetContext().GetProjectName())(
                                "logstore", GetContext().GetLogstoreName())("file", logPath));
            }
            LogtailAlarm::GetInstance()->SendAlarm(OUTDATED_LOG_ALARM,
                                                   std::string("logTime: ") + ToString(logTime.tv_sec),
                                                   GetContext().GetProjectName(),
                                                   GetContext().GetLogstoreName(),
                                                   GetContext().GetRegion());
        }
        ++(*mHistoryFailures);
        return false;
    }
    sourceEvent.SetTimestamp(logTime.tv_sec);
    if (mLegacyPreciseTimestampConfig.enabled) {
        int preciseTimestampLen = 0;
        do {
            ++preciseTimestampLen;
        } while ((preciseTimestamp /= 10) > 0);
        StringBuffer sb = sourceEvent.GetSourceBuffer()->AllocateStringBuffer(preciseTimestampLen + 1);
        snprintf(sb.data, sb.capacity, "%lu", preciseTimestamp);
        sourceEvent.SetContentNoCopy(mLegacyPreciseTimestampConfig.key, StringView(sb.data, sb.size));
    }
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
    const char* compareResult = strstr(mTimeFormat.c_str(), "%f");
    bool haveNanosecond = compareResult != nullptr;
    bool endWithNanosecond = compareResult == (mTimeFormat.c_str() + mTimeFormat.size() - 2);
    int nanosecondLength = -1;
    const char* strptimeResult = NULL;
    if ((!haveNanosecond || endWithNanosecond) && IsPrefixString(curTimeStr, timeStrCache)) {
        if (endWithNanosecond) {
            strptimeResult = Strptime(curTimeStr.data() + timeStrCache.length(), "%f", &logTime, nanosecondLength);
        } else {
            strptimeResult = curTimeStr.data() + timeStrCache.length();
            logTime.tv_nsec = 0;
        }
    } else {
        strptimeResult = Strptime(curTimeStr.data(), mTimeFormat.c_str(), &logTime, nanosecondLength, mSpecifiedYear);
        timeStrCache = curTimeStr.substr(0, curTimeStr.length() - nanosecondLength);
        logTime.tv_sec = logTime.tv_sec - mLogTimeZoneOffsetSecond;
    }
    if (NULL == strptimeResult) {
        if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                LOG_WARNING(sLogger,
                            ("parse time fail", curTimeStr)("project", GetContext().GetProjectName())(
                                "logstore", GetContext().GetLogstoreName())("file", logPath));
            }
            LogtailAlarm::GetInstance()->SendAlarm(PARSE_TIME_FAIL_ALARM,
                                                   curTimeStr.to_string() + " " + mTimeFormat,
                                                   GetContext().GetProjectName(),
                                                   GetContext().GetLogstoreName(),
                                                   GetContext().GetRegion());
        }

        ++(*mParseTimeFailures);
        return false;
    }

    if (mLegacyPreciseTimestampConfig.enabled) {
        if (nanosecondLength < 0) {
            preciseTimestamp = GetPreciseTimestamp(logTime.tv_sec, strptimeResult, mLegacyPreciseTimestampConfig);
        } else {
            preciseTimestamp = GetPreciseTimestampFromLogtailTime(logTime, mLegacyPreciseTimestampConfig);
        }
    }
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