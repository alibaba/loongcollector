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

#include "processor/ProcessorParseRegexNative.h"

#include "app_config/AppConfig.h"
#include "parser/LogParser.h" // for UNMATCH_LOG_KEY
#include "common/Constants.h"
#include "monitor/LogtailMetric.h"
#include "monitor/MetricConstants.h"
#include "common/TimeUtil.h"
#include "plugin/instance/ProcessorInstance.h"
#include "monitor/MetricConstants.h"

namespace logtail {
const std::string ProcessorParseRegexNative::sName = "processor_parse_regex_native";

bool ProcessorParseRegexNative::Init(const ComponentConfig& componentConfig) {
    const PipelineConfig& config = componentConfig.GetConfig();

    mSourceKey = DEFAULT_CONTENT_KEY;
    mDiscardUnmatch = config.mDiscardUnmatch;
    mUploadRawLog = config.mUploadRawLog;
    mRawLogTag = config.mAdvancedConfig.mRawLogTag;

    if (config.mRegs && config.mKeys) {
        std::list<std::string>::iterator regitr = config.mRegs->begin();
        std::list<std::string>::iterator keyitr = config.mKeys->begin();
        for (; regitr != config.mRegs->end() && keyitr != config.mKeys->end(); ++regitr, ++keyitr) {
            AddUserDefinedFormat(*regitr, *keyitr);
        }
        if (mUploadRawLog && mRawLogTag == mSourceKey) {
            mSourceKeyOverwritten = true;
        }
    }
    mParseFailures = &(GetContext().GetProcessProfile().parseFailures);
    mRegexMatchFailures = &(GetContext().GetProcessProfile().regexMatchFailures);
    mLogGroupSize = &(GetContext().GetProcessProfile().logGroupSize);

    mProcParseInSizeBytes = GetMetricsRecordRef().CreateCounter(METRIC_PROC_PARSE_IN_SIZE_BYTES);
    mProcParseOutSizeBytes = GetMetricsRecordRef().CreateCounter(METRIC_PROC_PARSE_OUT_SIZE_BYTES);
    mProcDiscardRecordsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PROC_DISCARD_RECORDS_TOTAL);
    mProcParseErrorTotal = GetMetricsRecordRef().CreateCounter(METRIC_PROC_PARSE_ERROR_TOTAL);
    mProcKeyCountNotMatchErrorTotal = GetMetricsRecordRef().CreateCounter(METRIC_PROC_KEY_COUNT_NOT_MATCH_ERROR_TOTAL);
    return true;
}


void ProcessorParseRegexNative::Process(PipelineEventGroup& logGroup) {
    if (logGroup.GetEvents().empty() || mUserDefinedFormat.empty()) {
        return;
    }
    const StringView& logPath = logGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED);
    EventsContainer& events = logGroup.MutableEvents();
    // works good normally. poor performance if most data need to be discarded.
    for (auto it = events.begin(); it != events.end();) {
        if (ProcessorParseRegexNative::ProcessEvent(logPath, *it)) {
            ++it;
        } else {
            it = events.erase(it);
        }
    }
    return;
}

bool ProcessorParseRegexNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<LogEvent>();
}

bool ProcessorParseRegexNative::ProcessEvent(const StringView& logPath, PipelineEventPtr& e) {
    if (!IsSupportedEvent(e)) {
        return true;
    }
    LogEvent& sourceEvent = e.Cast<LogEvent>();
    if (!sourceEvent.HasContent(mSourceKey)) {
        return true;
    }
    auto rawContent = sourceEvent.GetContent(mSourceKey);
    bool res = true;
    for (uint32_t i = 0; i < mUserDefinedFormat.size(); ++i) { // support multiple patterns
        const UserDefinedFormat& format = mUserDefinedFormat[i];
        if (format.mIsWholeLineMode) {
            res = WholeLineModeParser(sourceEvent, format.mKeys.empty() ? DEFAULT_CONTENT_KEY : format.mKeys[0]);
        } else {
            res = RegexLogLineParser(sourceEvent, format.mReg, format.mKeys, logPath);
        }
        if (res) {
            break;
        }
    }
    if (!res && !mDiscardUnmatch) {
        AddLog(LogParser::UNMATCH_LOG_KEY, // __raw_log__
               rawContent,
               sourceEvent); // legacy behavior, should use sourceKey
    }
    if (res || !mDiscardUnmatch) {
        if (mUploadRawLog && (!res || !mRawLogTagOverwritten)) {
            AddLog(mRawLogTag, rawContent, sourceEvent); // __raw__
        }
        if (res && !mSourceKeyOverwritten) {
            sourceEvent.DelContent(mSourceKey);
        }
        return true;
    }
    mProcDiscardRecordsTotal->Add(1);
    return false;
}

void ProcessorParseRegexNative::AddUserDefinedFormat(const std::string& regStr, const std::string& keys) {
    std::vector<std::string> keyParts = StringSpliter(keys, ",");
    for (auto& it : keyParts) {
        if (it == mSourceKey) {
            mSourceKeyOverwritten = true;
        }
        if (it == mRawLogTag) {
            mRawLogTagOverwritten = true;
        }
    }
    boost::regex reg(regStr);
    bool isWholeLineMode = regStr == "(.*)";
    mUserDefinedFormat.push_back(UserDefinedFormat(reg, keyParts, isWholeLineMode));
}

bool ProcessorParseRegexNative::WholeLineModeParser(LogEvent& sourceEvent, const std::string& key) {
    StringView buffer = sourceEvent.GetContent(mSourceKey);
    AddLog(StringView(key), buffer, sourceEvent);
    mProcParseInSizeBytes->Add(buffer.size());
    return true;
}

void ProcessorParseRegexNative::AddLog(const StringView& key, const StringView& value, LogEvent& targetEvent) {
    targetEvent.SetContentNoCopy(key, value);
    size_t keyValueSize = key.size() + value.size();
    *mLogGroupSize += keyValueSize + 5;
    mProcParseOutSizeBytes->Add(keyValueSize);
}

bool ProcessorParseRegexNative::RegexLogLineParser(LogEvent& sourceEvent,
                                                   const boost::regex& reg,
                                                   const std::vector<std::string>& keys,
                                                   const StringView& logPath) {
    boost::match_results<const char*> what;
    std::string exception;
    StringView buffer = sourceEvent.GetContent(mSourceKey);
    bool parseSuccess = true;
    mProcParseInSizeBytes->Add(buffer.size());
    if (!BoostRegexMatch(buffer.data(), buffer.size(), reg, exception, what, boost::match_default)) {
        if (!exception.empty()) {
            if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
                if (GetContext().GetAlarm().IsLowLevelAlarmValid()) {
                    LOG_ERROR(GetContext().GetLogger(),
                              ("parse regex log fail", buffer)("exception", exception)("project",
                                                                                       GetContext().GetProjectName())(
                                  "logstore", GetContext().GetLogstoreName())("file", logPath));
                }
                GetContext().GetAlarm().SendAlarm(REGEX_MATCH_ALARM,
                                                  "errorlog:" + buffer.to_string() + " | exception:" + exception,
                                                  GetContext().GetProjectName(),
                                                  GetContext().GetLogstoreName(),
                                                  GetContext().GetRegion());
            }
        } else {
            if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
                if (GetContext().GetAlarm().IsLowLevelAlarmValid()) {
                    LOG_WARNING(GetContext().GetLogger(),
                                ("parse regex log fail", buffer)("project", GetContext().GetProjectName())(
                                    "logstore", GetContext().GetLogstoreName())("file", logPath));
                }
                GetContext().GetAlarm().SendAlarm(REGEX_MATCH_ALARM,
                                                  std::string("errorlog:") + buffer.to_string(),
                                                  GetContext().GetProjectName(),
                                                  GetContext().GetLogstoreName(),
                                                  GetContext().GetRegion());
            }
        }
        ++(*mRegexMatchFailures);
        ++(*mParseFailures);
        mProcParseErrorTotal->Add(1);
        parseSuccess = false;
    } else if (what.size() <= keys.size()) {
        if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            if (GetContext().GetAlarm().IsLowLevelAlarmValid()) {
                LOG_WARNING(GetContext().GetLogger(),
                            ("parse key count not match",
                             what.size())("parse regex log fail", buffer)("project", GetContext().GetProjectName())(
                                "logstore", GetContext().GetLogstoreName())("file", logPath));
            }
            GetContext().GetAlarm().SendAlarm(REGEX_MATCH_ALARM,
                                              "parse key count not match" + ToString(what.size())
                                                  + "errorlog:" + buffer.to_string(),
                                              GetContext().GetProjectName(),
                                              GetContext().GetLogstoreName(),
                                              GetContext().GetRegion());
        }
        ++(*mRegexMatchFailures);
        ++(*mParseFailures);
        mProcKeyCountNotMatchErrorTotal->Add(1);
        parseSuccess = false;
    }
    if (!parseSuccess) {
        return false;
    }

    for (uint32_t i = 0; i < keys.size(); i++) {
        AddLog(keys[i], StringView(what[i + 1].begin(), what[i + 1].length()), sourceEvent);
    }
    return true;
}

} // namespace logtail