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

#include "plugin/processor/ProcessorParseRegexNative.h"

#include "app_config/AppConfig.h"
#include "common/ParamExtractor.h"
#include "monitor/metric_constants/MetricConstants.h"

namespace logtail {

const std::string ProcessorParseRegexNative::sName = "processor_parse_regex_native";

bool ProcessorParseRegexNative::Init(const Json::Value& config) {
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

    // Regex
    if (!GetMandatoryStringParam(config, "Regex", mRegex, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    } else if (!IsRegexValid(mRegex)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           "mandatory string param Regex is not a valid regex",
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    mReg = boost::regex(mRegex);
    mIsWholeLineMode = mRegex == "(.*)";

    // Keys
    if (!GetMandatoryListParam(config, "Keys", mKeys, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    // Since the 'keys' field in old logtail config is an array with a single comma separated string inside (e.g.,
    // ["k1,k2,k3"]), which is different from openAPI, chances are the 'key' field in openAPI is unintentionally set to
    // the 'keys' field in logtail config. However, such wrong format can still work in logtail due to the conversion
    // done in the server, which simply concatenates all strings in the array with comma. Therefor, to be compatibal
    // with such wrong behavior, we must explicitly allow such format.
    if (mKeys.size() == 1 && mKeys[0].find(',') != std::string::npos) {
        mKeys = SplitString(mKeys[0], ",");
    }
    for (const auto& it : mKeys) {
        if (it == mSourceKey) {
            mSourceKeyOverwritten = true;
            break;
        }
    }

    if (!mCommonParserOptions.Init(config, *mContext, sName)) {
        return false;
    }

    mDiscardedEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_DISCARDED_EVENTS_TOTAL);
    mOutFailedEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_FAILED_EVENTS_TOTAL);
    mOutKeyNotFoundEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_KEY_NOT_FOUND_EVENTS_TOTAL);
    mOutSuccessfulEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_SUCCESSFUL_EVENTS_TOTAL);

    return true;
}

void ProcessorParseRegexNative::Process(PipelineEventGroup& logGroup) {
    if (logGroup.GetEvents().empty()) {
        return;
    }
    const StringView& logPath = logGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED);
    EventsContainer& events = logGroup.MutableEvents();

    size_t wIdx = 0;
    for (size_t rIdx = 0; rIdx < events.size(); ++rIdx) {
        if (ProcessEvent(logPath, events[rIdx])) {
            if (wIdx != rIdx) {
                events[wIdx] = std::move(events[rIdx]);
            }
            ++wIdx;
        }
    }
    events.resize(wIdx);
    return;
}

bool ProcessorParseRegexNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<LogEvent>();
}

bool ProcessorParseRegexNative::ProcessEvent(const StringView& logPath, PipelineEventPtr& e) {
    if (!IsSupportedEvent(e)) {
        mOutFailedEventsTotal->Add(1);
        return true;
    }
    LogEvent& sourceEvent = e.Cast<LogEvent>();
    if (!sourceEvent.HasContent(mSourceKey)) {
        mOutKeyNotFoundEventsTotal->Add(1);
        return true;
    }
    auto rawContent = sourceEvent.GetContent(mSourceKey);
    bool parseSuccess = true;

    if (mIsWholeLineMode) {
        parseSuccess = WholeLineModeParser(sourceEvent, mKeys.empty() ? DEFAULT_CONTENT_KEY : mKeys[0]);
    } else {
        parseSuccess = RegexLogLineParser(sourceEvent, mReg, mKeys, logPath);
    }

    if (!parseSuccess || !mSourceKeyOverwritten) {
        sourceEvent.DelContent(mSourceKey);
    }
    if (mCommonParserOptions.ShouldAddSourceContent(parseSuccess)) {
        AddLog(mCommonParserOptions.mRenamedSourceKey, rawContent, sourceEvent, false);
    }
    if (mCommonParserOptions.ShouldAddLegacyUnmatchedRawLog(parseSuccess)) {
        AddLog(mCommonParserOptions.legacyUnmatchedRawLogKey, rawContent, sourceEvent, false);
    }
    if (mCommonParserOptions.ShouldEraseEvent(parseSuccess, sourceEvent)) {
        mDiscardedEventsTotal->Add(1);
        return false;
    }
    mOutSuccessfulEventsTotal->Add(1);
    return true;
}

bool ProcessorParseRegexNative::WholeLineModeParser(LogEvent& sourceEvent, const std::string& key) {
    StringView buffer = sourceEvent.GetContent(mSourceKey);
    AddLog(StringView(key), buffer, sourceEvent);
    return true;
}

void ProcessorParseRegexNative::AddLog(const StringView& key,
                                       const StringView& value,
                                       LogEvent& targetEvent,
                                       bool overwritten) {
    if (!overwritten && targetEvent.HasContent(key)) {
        return;
    }
    targetEvent.SetContentNoCopy(key, value);
}

bool ProcessorParseRegexNative::RegexLogLineParser(LogEvent& sourceEvent,
                                                   const boost::regex& reg,
                                                   const std::vector<std::string>& keys,
                                                   const StringView& logPath) {
    boost::match_results<const char*> what;
    std::string exception;
    StringView buffer = sourceEvent.GetContent(mSourceKey);
    bool parseSuccess = true;
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
        mOutFailedEventsTotal->Add(1);
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
