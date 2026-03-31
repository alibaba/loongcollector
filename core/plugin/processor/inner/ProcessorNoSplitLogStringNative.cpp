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

#include "plugin/processor/inner/ProcessorNoSplitLogStringNative.h"

#include <string>

#include "common/ParamExtractor.h"
#include "constants/TagConstants.h"
#include "logger/Logger.h"
#include "models/LogEvent.h"
#include "models/PipelineEventGroup.h"
#include "monitor/metric_constants/MetricConstants.h"

namespace logtail {

const std::string ProcessorNoSplitLogStringNative::sName = "processor_no_split_string_native";

bool ProcessorNoSplitLogStringNative::Init(const Json::Value& config) {
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

    mMatchedEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_MATCHED_EVENTS_TOTAL);
    mMatchedLinesTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_MATCHED_LINES_TOTAL);

    return true;
}

/*
    Presumption:
    1. Event must be LogEvent
    2. Log content must have exactly 1 field (sourceKey)
    3. The last \n of each log string is discarded in LogFileReader

    Unlike ProcessorSplitMultilineLogStringNative, this processor does NOT split
    by lines or patterns. The entire content is treated as a single log entry.
*/
void ProcessorNoSplitLogStringNative::Process(PipelineEventGroup& logGroup) {
    if (logGroup.GetEvents().empty()) {
        return;
    }
    EventsContainer newEvents;
    for (PipelineEventPtr& e : logGroup.MutableEvents()) {
        ProcessEvent(logGroup, std::move(e), newEvents);
    }
    logGroup.SwapEvents(newEvents);
}

bool ProcessorNoSplitLogStringNative::IsSupportedEvent(const PipelineEventPtr& e) const {
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

void ProcessorNoSplitLogStringNative::ProcessEvent(PipelineEventGroup& logGroup,
                                                    PipelineEventPtr&& e,
                                                    EventsContainer& newEvents) {
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

    // Count lines in source content
    int lineCount = 1;
    for (size_t i = 0; i < sourceVal.size(); ++i) {
        if (sourceVal[i] == '\n') {
            ++lineCount;
        }
    }
    ADD_COUNTER(mMatchedLinesTotal, lineCount);

    // No splitting: treat the entire content as one log entry
    CreateNewEvent(sourceVal, sourceKey, sourceEvent, logGroup, newEvents);
    ADD_COUNTER(mMatchedEventsTotal, 1);
}

void ProcessorNoSplitLogStringNative::CreateNewEvent(const StringView& content,
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
        targetEvent->SetTimestamp(sourceEvent.GetTimestamp(), sourceEvent.GetTimestampNanosecond());
        auto const offset = sourceEvent.GetPosition().first + (content.data() - sourceVal.data());
        auto const length = sourceEvent.GetPosition().second - (content.data() - sourceVal.data());
        targetEvent->SetPosition(offset, length);
        if (logGroup.HasMetadata(EventGroupMetaKey::LOG_FILE_OFFSET_KEY)) {
            StringBuffer offsetStr = logGroup.GetSourceBuffer()->CopyString(ToString(offset));
            targetEvent->SetContentNoCopy(logGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_OFFSET_KEY),
                                          StringView(offsetStr.data, offsetStr.size));
        }
        newEvents.emplace_back(std::move(targetEvent), true, nullptr);
    }
}

} // namespace logtail
