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

#include "processor/inner/ProcessorSplitLogStringNative.h"

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

    // AppendingLogPositionMeta
    if (!GetOptionalBoolParam(config, "AppendingLogPositionMeta", mAppendingLogPositionMeta, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mAppendingLogPositionMeta,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    mSplitLines = &(GetContext().GetProcessProfile().splitLines);

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
    *mSplitLines = newEvents.size();
    logGroup.SwapEvents(newEvents);

    return;
}

bool ProcessorSplitLogStringNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<LogEvent>();
}

void ProcessorSplitLogStringNative::ProcessEvent(PipelineEventGroup& logGroup,
                                                 PipelineEventPtr&& e,
                                                 EventsContainer& newEvents) {
    if (!IsSupportedEvent(e)) {
        newEvents.emplace_back(std::move(e));
        return;
    }

    LogEvent& sourceEvent = e.Cast<LogEvent>();
    if (!sourceEvent.HasContent(mSourceKey)) {
        newEvents.emplace_back(std::move(e));
        return;
    }

    StringView sourceVal = sourceEvent.GetContent(mSourceKey);
    StringBuffer sourceKey = logGroup.GetSourceBuffer()->CopyString(mSourceKey);
    long sourceOffset = 0L;
    if (sourceEvent.HasContent(LOG_RESERVED_KEY_FILE_OFFSET)) {
        sourceOffset = atol(sourceEvent.GetContent(LOG_RESERVED_KEY_FILE_OFFSET).data()); // use safer method
    }

    size_t begin = 0;
    while (begin < sourceVal.size()) {
        std::unique_ptr<LogEvent> targetEvent = logGroup.CreateLogEvent();
        StringView content = GetNextLine(sourceVal, begin);
        targetEvent->SetContentNoCopy(StringView(sourceKey.data, sourceKey.size), content);
        targetEvent->SetTimestamp(
            sourceEvent.GetTimestamp(),
            sourceEvent.GetTimestampNanosecond()); // it is easy to forget other fields, better solution?
        if (mAppendingLogPositionMeta) {
            auto const offset = sourceOffset + (content.data() - sourceVal.data());
            StringBuffer offsetStr = logGroup.GetSourceBuffer()->CopyString(std::to_string(offset));
            targetEvent->SetContentNoCopy(LOG_RESERVED_KEY_FILE_OFFSET, StringView(offsetStr.data, offsetStr.size));
        }
        if (sourceEvent.Size() > 1) { // copy other fields
            for (const auto& kv : sourceEvent) {
                if (kv.first != mSourceKey && kv.first != LOG_RESERVED_KEY_FILE_OFFSET) {
                    targetEvent->SetContentNoCopy(kv.first, kv.second);
                }
            }
        }
        newEvents.emplace_back(std::move(targetEvent));
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
