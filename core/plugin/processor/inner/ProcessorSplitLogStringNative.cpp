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

#include "plugin/processor/inner/ProcessorSplitLogStringNative.h"

#include "common/ParamExtractor.h"
#include "models/LogEvent.h"

namespace logtail {

const std::string ProcessorSplitLogStringNative::sName = "processor_split_string_native";

bool ProcessorSplitLogStringNative::Init(const Json::Value& config) {
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

    // SplitChar
    int32_t splitter = '\n';
    if (!GetOptionalIntParam(config, "SplitChar", splitter, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              "\\n",
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    } else {
        mSplitChar = static_cast<char>(splitter);
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

    return true;
}

void ProcessorSplitLogStringNative::Process(PipelineEventGroup& logGroup) {
    if (logGroup.GetEvents().empty()) {
        return;
    }
    EventsContainer newEvents;
    for (PipelineEventPtr& e : logGroup.MutableEvents()) {
        ProcessEvent(logGroup, std::move(e), newEvents);
    }
    logGroup.SwapEvents(newEvents);
}

bool ProcessorSplitLogStringNative::IsSupportedEvent(const PipelineEventPtr& e) const {
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

void ProcessorSplitLogStringNative::ProcessEvent(PipelineEventGroup& logGroup,
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

    size_t begin = 0;
    while (begin < sourceVal.size()) {
        StringView content = GetNextLine(sourceVal, begin);
        if (mEnableRawContent) {
            std::unique_ptr<RawEvent> targetEvent = logGroup.CreateRawEvent(true);
            targetEvent->SetContentNoCopy(content);
            targetEvent->SetTimestamp(sourceEvent.GetTimestamp(), sourceEvent.GetTimestampNanosecond());
            newEvents.emplace_back(std::move(targetEvent), true, nullptr);
        } else {
            std::unique_ptr<LogEvent> targetEvent = logGroup.CreateLogEvent(true);
            targetEvent->SetContentNoCopy(StringView(sourceKey.data, sourceKey.size), content);
            targetEvent->SetTimestamp(
                sourceEvent.GetTimestamp(),
                sourceEvent.GetTimestampNanosecond()); // it is easy to forget other fields, better solution?
            auto const offset = sourceEvent.GetPosition().first + (content.data() - sourceVal.data());
            auto const length = begin + content.size() == sourceVal.size()
                ? sourceEvent.GetPosition().second - (content.data() - sourceVal.data())
                : content.size() + 1;
            targetEvent->SetPosition(offset, length);
            if (logGroup.HasMetadata(EventGroupMetaKey::LOG_FILE_OFFSET_KEY)) {
                StringBuffer offsetStr = logGroup.GetSourceBuffer()->CopyString(ToString(offset));
                targetEvent->SetContentNoCopy(logGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_OFFSET_KEY),
                                              StringView(offsetStr.data, offsetStr.size));
            }
            newEvents.emplace_back(std::move(targetEvent), true, nullptr);
        }
        begin += content.size() + 1;
    }
}

StringView ProcessorSplitLogStringNative::GetNextLine(StringView log, size_t begin) {
    if (begin >= log.size()) {
        return StringView();
    }

    for (size_t end = begin; end < log.size(); ++end) {
        if (log[end] == mSplitChar) {
            return StringView(log.data() + begin, end - begin);
        }
    }
    return StringView(log.data() + begin, log.size() - begin);
}

} // namespace logtail
