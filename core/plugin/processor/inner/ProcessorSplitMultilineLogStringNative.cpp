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

#include "plugin/processor/inner/ProcessorSplitMultilineLogStringNative.h"

#include <string>

#include "boost/regex.hpp"

#include "app_config/AppConfig.h"
#include "collection_pipeline/plugin/instance/ProcessorInstance.h"
#include "common/ParamExtractor.h"
#include "constants/Constants.h"
#include "constants/TagConstants.h"
#include "logger/Logger.h"
#include "models/LogEvent.h"
#include "models/PipelineEventGroup.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "runner/ProcessorRunner.h"

namespace logtail {

const std::string ProcessorSplitMultilineLogStringNative::sName = "processor_split_multiline_log_string_native";

bool ProcessorSplitMultilineLogStringNative::Init(const Json::Value& config) {
    std::string errorMsg;

    // SourceKey
    if (!GetOptionalStringParam(config, "SourceKey", mSourceKey, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mSourceKey,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    if (!mMultiline.Init(config, *mContext, sName)) {
        return false;
    }

    // EnableRawContent
    if (!GetOptionalBoolParam(config, "EnableRawContent", mEnableRawContent, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mEnableRawContent,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    for (int i = 0; i < AppConfig::GetInstance()->GetProcessThreadCount(); ++i) {
        if (!mMultiline.mStartPattern.empty()) {
            mStartPatternReg.emplace_back(mMultiline.mStartPattern);
        }
        if (!mMultiline.mContinuePattern.empty()) {
            mContinuePatternReg.emplace_back(mMultiline.mContinuePattern);
        }
        if (!mMultiline.mEndPattern.empty()) {
            mEndPatternReg.emplace_back(mMultiline.mEndPattern);
        }
    }

    mMatchedEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_MATCHED_EVENTS_TOTAL);
    mMatchedLinesTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_MATCHED_LINES_TOTAL);
    mUnmatchedLinesTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_UNMATCHED_LINES_TOTAL);

    return true;
}

/*
    Presumption:
    1. Event must be LogEvent
    2. Log content must have exactly 1 field (sourceKey)
    3. The last \n of each log string is discarded in LogFileReader
*/
void ProcessorSplitMultilineLogStringNative::Process(PipelineEventGroup& logGroup) {
    if (logGroup.GetEvents().empty()) {
        return;
    }
    int inputLines = 0;
    int unmatchLines = 0;
    EventsContainer newEvents;
    StringView logPath = logGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED);
    for (PipelineEventPtr& e : logGroup.MutableEvents()) {
        ProcessEvent(logGroup, logPath, std::move(e), newEvents, &inputLines, &unmatchLines);
    }
    ADD_COUNTER(mMatchedLinesTotal, inputLines - unmatchLines);
    ADD_COUNTER(mUnmatchedLinesTotal, unmatchLines);
    logGroup.SwapEvents(newEvents);
}

bool ProcessorSplitMultilineLogStringNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    if (e.Is<LogEvent>()) {
        return true;
    }
    LOG_ERROR(mContext->GetLogger(),
              ("unexpected error", "unsupported log event")("processor", sName)("config", mContext->GetConfigName()));
    mContext->GetAlarm().SendAlarmWarning(SPLIT_LOG_FAIL_ALARM,
                                          "unexpected error: unsupported log event.\tprocessor: " + sName
                                              + "\tconfig: " + mContext->GetConfigName(),
                                          mContext->GetRegion(),
                                          mContext->GetProjectName(),
                                          mContext->GetConfigName(),
                                          mContext->GetLogstoreName());
    return false;
}

void ProcessorSplitMultilineLogStringNative::ProcessEvent(PipelineEventGroup& logGroup,
                                                          StringView logPath,
                                                          PipelineEventPtr&& e,
                                                          EventsContainer& newEvents,
                                                          int* inputLines,
                                                          int* unmatchLines) {
    if (!IsSupportedEvent(e)) {
        newEvents.emplace_back(std::move(e));
        return;
    }
    LogEvent& sourceEvent = e.Cast<LogEvent>();

    std::string errorMsg;
    if (sourceEvent.Size() != 1) {
        errorMsg = "log event fields cnt does not equal to 1";
    } else if (!sourceEvent.HasContent(mSourceKey)) {
        errorMsg = "log event does not have content key";
    }
    if (!errorMsg.empty()) {
        newEvents.emplace_back(std::move(e));
        LOG_ERROR(mContext->GetLogger(),
                  ("unexpected error", errorMsg)("processor", sName)("config", mContext->GetConfigName()));
        mContext->GetAlarm().SendAlarmWarning(SPLIT_LOG_FAIL_ALARM,
                                              "unexpected error: " + errorMsg + ".\tprocessor: " + sName
                                                  + "\tconfig: " + mContext->GetConfigName(),
                                              mContext->GetRegion(),
                                              mContext->GetProjectName(),
                                              mContext->GetConfigName(),
                                              mContext->GetLogstoreName());
        return;
    }

    StringView sourceVal = sourceEvent.GetContent(mSourceKey);
    StringBuffer sourceKey = logGroup.GetSourceBuffer()->CopyString(mSourceKey);

    std::string exception;
    const char* multiStartIndex = nullptr;
    bool isPartialLog = false;
    if (!HasStartPattern() && !HasContinuePattern() && HasEndPattern()) {
        // if only end pattern is given, then it will stick to this state
        isPartialLog = true;
        multiStartIndex = sourceVal.data();
    }

    size_t begin = 0;
    while (begin < sourceVal.size()) {
        StringView content = GetNextLine(sourceVal, begin);
        bool isLastLog = begin + content.size() == sourceVal.size();
        ++(*inputLines);
        if (!isPartialLog) {
            // it is impossible to enter this state if only end pattern is given
            boost::regex regex;
            if (HasStartPattern()) {
                regex = GetStartPatternReg();
            } else {
                regex = GetContinuePatternReg();
            }
            if (BoostRegexSearch(content.data(), content.size(), regex, exception)) {
                multiStartIndex = content.data();
                isPartialLog = true;
            } else if (HasEndPattern() && !HasStartPattern() && HasContinuePattern()
                       && BoostRegexSearch(content.data(), content.size(), GetEndPatternReg(), exception)) {
                // case: continue + end
                CreateNewEvent(content, isLastLog, sourceKey, sourceEvent, logGroup, newEvents);
                multiStartIndex = content.data() + content.size() + 1;
                ADD_COUNTER(mMatchedEventsTotal, 1);
            } else {
                HandleUnmatchLogs(
                    content, isLastLog, sourceKey, sourceEvent, logGroup, newEvents, logPath, unmatchLines);
            }
        } else {
            // case: start + continue or continue + end
            if (HasContinuePattern()
                && BoostRegexSearch(content.data(), content.size(), GetContinuePatternReg(), exception)) {
                begin += content.size() + 1;
                continue;
            }
            if (HasEndPattern()) {
                // case: start + end or continue + end or end
                if (HasContinuePattern()) {
                    // current line is not matched against the continue pattern, so the end pattern will decide
                    // if the current log is a match or not
                    if (BoostRegexSearch(content.data(), content.size(), GetEndPatternReg(), exception)) {
                        CreateNewEvent(StringView(multiStartIndex, content.data() + content.size() - multiStartIndex),
                                       isLastLog,
                                       sourceKey,
                                       sourceEvent,
                                       logGroup,
                                       newEvents);
                        ADD_COUNTER(mMatchedEventsTotal, 1);
                    } else {
                        HandleUnmatchLogs(
                            StringView(multiStartIndex, content.data() + content.size() - multiStartIndex),
                            isLastLog,
                            sourceKey,
                            sourceEvent,
                            logGroup,
                            newEvents,
                            logPath,
                            unmatchLines);
                    }
                    isPartialLog = false;
                } else {
                    // case: start + end or end
                    if (BoostRegexSearch(content.data(), content.size(), GetEndPatternReg(), exception)) {
                        CreateNewEvent(StringView(multiStartIndex, content.data() + content.size() - multiStartIndex),
                                       isLastLog,
                                       sourceKey,
                                       sourceEvent,
                                       logGroup,
                                       newEvents);
                        if (HasStartPattern()) {
                            isPartialLog = false;
                        } else {
                            multiStartIndex = content.data() + content.size() + 1;
                        }
                        ADD_COUNTER(mMatchedEventsTotal, 1);
                        // if only end pattern is given, start another log automatically
                    }
                    // no continue pattern given, and the current line in not matched against the end pattern,
                    // so wait for the next line
                }
            } else {
                if (!HasContinuePattern()) {
                    // case: start
                    if (BoostRegexSearch(content.data(), content.size(), GetStartPatternReg(), exception)) {
                        CreateNewEvent(StringView(multiStartIndex, content.data() - 1 - multiStartIndex),
                                       isLastLog,
                                       sourceKey,
                                       sourceEvent,
                                       logGroup,
                                       newEvents);
                        multiStartIndex = content.data();
                        ADD_COUNTER(mMatchedEventsTotal, 1);
                    }
                } else {
                    // case: start + continue
                    // continue pattern is given, but current line is not matched against the continue pattern
                    CreateNewEvent(StringView(multiStartIndex, content.data() - 1 - multiStartIndex),
                                   isLastLog,
                                   sourceKey,
                                   sourceEvent,
                                   logGroup,
                                   newEvents);
                    ADD_COUNTER(mMatchedEventsTotal, 1);
                    if (!BoostRegexSearch(content.data(), content.size(), GetStartPatternReg(), exception)) {
                        // when no end pattern is given, the only chance to enter unmatched state is when both
                        // start and continue pattern are given, and the current line is not matched against the
                        // start pattern
                        HandleUnmatchLogs(
                            content, isLastLog, sourceKey, sourceEvent, logGroup, newEvents, logPath, unmatchLines);
                        isPartialLog = false;
                    } else {
                        multiStartIndex = content.data();
                    }
                }
            }
        }
        begin += content.size() + 1;
    }
    // when in unmatched state, the unmatched log is handled one by one, so there is no need for additional handle
    // here
    if (isPartialLog && multiStartIndex - sourceVal.data() < static_cast<int64_t>(sourceVal.size())) {
        if (!HasEndPattern()) {
            CreateNewEvent(StringView(multiStartIndex, sourceVal.data() + sourceVal.size() - multiStartIndex),
                           true,
                           sourceKey,
                           sourceEvent,
                           logGroup,
                           newEvents);
            ADD_COUNTER(mMatchedEventsTotal, 1);
        } else {
            HandleUnmatchLogs(StringView(multiStartIndex, sourceVal.data() + sourceVal.size() - multiStartIndex),
                              true,
                              sourceKey,
                              sourceEvent,
                              logGroup,
                              newEvents,
                              logPath,
                              unmatchLines);
        }
    }
}

void ProcessorSplitMultilineLogStringNative::CreateNewEvent(const StringView& content,
                                                            bool isLastLog,
                                                            StringBuffer& sourceKey,
                                                            const LogEvent& sourceEvent,
                                                            PipelineEventGroup& logGroup,
                                                            EventsContainer& newEvents) {
    if (mEnableRawContent) {
        std::unique_ptr<RawEvent> targetEvent = logGroup.CreateRawEvent(true);
        targetEvent->SetContentNoCopy(content);
        targetEvent->SetTimestamp(sourceEvent.GetTimestamp(), sourceEvent.GetTimestampNanosecond());
        newEvents.emplace_back(std::move(targetEvent), true, nullptr);
    } else {
        StringView sourceVal = sourceEvent.GetContent(mSourceKey);
        std::unique_ptr<LogEvent> targetEvent = logGroup.CreateLogEvent(true);
        targetEvent->SetContentNoCopy(StringView(sourceKey.data, sourceKey.size), content);
        targetEvent->SetTimestamp(
            sourceEvent.GetTimestamp(),
            sourceEvent.GetTimestampNanosecond()); // it is easy to forget other fields, better solution?
        auto const offset = sourceEvent.GetPosition().first + (content.data() - sourceVal.data());
        auto const length
            = isLastLog ? sourceEvent.GetPosition().second - (content.data() - sourceVal.data()) : content.size() + 1;
        targetEvent->SetPosition(offset, length);
        if (logGroup.HasMetadata(EventGroupMetaKey::LOG_FILE_OFFSET_KEY)) {
            StringBuffer offsetStr = logGroup.GetSourceBuffer()->CopyString(ToString(offset));
            targetEvent->SetContentNoCopy(logGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_OFFSET_KEY),
                                          StringView(offsetStr.data, offsetStr.size));
        }
        newEvents.emplace_back(std::move(targetEvent), true, nullptr);
    }
}

void ProcessorSplitMultilineLogStringNative::HandleUnmatchLogs(const StringView& sourceVal,
                                                               bool isLastLog,
                                                               StringBuffer& sourceKey,
                                                               const LogEvent& sourceEvent,
                                                               PipelineEventGroup& logGroup,
                                                               EventsContainer& newEvents,
                                                               StringView logPath,
                                                               int* unmatchLines) {
    size_t begin = 0, fisrtLogSize = 0, totalLines = 0;
    while (begin < sourceVal.size()) {
        StringView content = GetNextLine(sourceVal, begin);
        ++(*unmatchLines);
        if (mMultiline.mUnmatchedContentTreatment == MultilineOptions::UnmatchedContentTreatment::SINGLE_LINE) {
            CreateNewEvent(content, isLastLog, sourceKey, sourceEvent, logGroup, newEvents);
        }
        begin += content.size() + 1;
        ++totalLines;
        if (fisrtLogSize == 0) {
            fisrtLogSize = content.size();
        }
    }
    if (!mMultiline.mIgnoringUnmatchWarning && AlarmManager::GetInstance()->IsLowLevelAlarmValid()) {
        LOG_WARNING(mContext->GetLogger(),
                    ("unmatched log string", "please check regex")(
                        "action", UnmatchedContentTreatmentToString(mMultiline.mUnmatchedContentTreatment))(
                        "first line:", sourceVal.substr(0, fisrtLogSize).to_string())("filepath", logPath.to_string())(
                        "processor", sName)("config", mContext->GetConfigName())("total lines", totalLines)(
                        "log bytes", sourceVal.size() + 1));
        mContext->GetAlarm().SendAlarmWarning(
            SPLIT_LOG_FAIL_ALARM,
            "unmatched log string, first line:" + sourceVal.substr(0, fisrtLogSize).to_string() + "\taction: "
                + UnmatchedContentTreatmentToString(mMultiline.mUnmatchedContentTreatment) + "\tfilepath: "
                + logPath.to_string() + "\tprocessor: " + sName + "\tconfig: " + mContext->GetConfigName(),
            mContext->GetRegion(),
            mContext->GetProjectName(),
            mContext->GetConfigName(),
            mContext->GetLogstoreName());
    }
}

StringView ProcessorSplitMultilineLogStringNative::GetNextLine(StringView log, size_t begin) {
    if (begin >= log.size()) {
        return StringView();
    }

    for (size_t end = begin; end < log.size(); ++end) {
        if (log[end] == '\n') {
            return StringView(log.data() + begin, end - begin);
        }
    }
    return StringView(log.data() + begin, log.size() - begin);
}

const boost::regex& ProcessorSplitMultilineLogStringNative::GetStartPatternReg() const {
    return mStartPatternReg[ProcessorRunner::GetThreadNo()];
}

const boost::regex& ProcessorSplitMultilineLogStringNative::GetContinuePatternReg() const {
    return mContinuePatternReg[ProcessorRunner::GetThreadNo()];
}

const boost::regex& ProcessorSplitMultilineLogStringNative::GetEndPatternReg() const {
    return mEndPatternReg[ProcessorRunner::GetThreadNo()];
}

} // namespace logtail
