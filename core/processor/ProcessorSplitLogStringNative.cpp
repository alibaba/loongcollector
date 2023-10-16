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

#include "processor/ProcessorSplitLogStringNative.h"
#include "common/Constants.h"
#include "models/LogEvent.h"
#include "plugin/instance/ProcessorInstance.h"
#include "monitor/MetricConstants.h"


namespace logtail {
const std::string ProcessorSplitLogStringNative::sName = "processor_split_string_native";

bool ProcessorSplitLogStringNative::Init(const ComponentConfig& componentConfig) {
    const PipelineConfig& config = componentConfig.GetConfig();

    mSplitKey = DEFAULT_CONTENT_KEY;
    mSplitChar = config.mLogType == JSON_LOG ? '\0' : '\n';
    mEnableLogPositionMeta = config.mAdvancedConfig.mEnableLogPositionMeta;
    mFeedLines = &(GetContext().GetProcessProfile().feedLines);
    mSplitLines = &(GetContext().GetProcessProfile().splitLines);

    return true;
}

void ProcessorSplitLogStringNative::Process(PipelineEventGroup& logGroup) {
    if (logGroup.GetEvents().empty()) {
        return;
    }
    EventsContainer newEvents;
    for (const PipelineEventPtr& e : logGroup.GetEvents()) {
        ProcessEvent(logGroup, e, newEvents);
    }
    *mSplitLines = newEvents.size();
    logGroup.SwapEvents(newEvents);

    return;
}

bool ProcessorSplitLogStringNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<LogEvent>();
}

void ProcessorSplitLogStringNative::ProcessEvent(PipelineEventGroup& logGroup,
                                                 const PipelineEventPtr& e,
                                                 EventsContainer& newEvents) {
    if (!IsSupportedEvent(e)) {
        newEvents.emplace_back(e);
        return;
    }
    const LogEvent& sourceEvent = e.Cast<LogEvent>();
    if (!sourceEvent.HasContent(mSplitKey)) {
        newEvents.emplace_back(e);
        return;
    }
    StringView sourceVal = sourceEvent.GetContent(mSplitKey);
    std::vector<StringView> logIndex; // all splitted logs
    int feedLines = 0;
    LogSplit(sourceVal.data(), sourceVal.size(), feedLines, logIndex);
    *mFeedLines += feedLines;

    long sourceoffset = 0L;
    if (sourceEvent.HasContent(LOG_RESERVED_KEY_FILE_OFFSET)) {
        sourceoffset = atol(sourceEvent.GetContent(LOG_RESERVED_KEY_FILE_OFFSET).data()); // use safer method
    }
    StringBuffer splitKey = logGroup.GetSourceBuffer()->CopyString(mSplitKey);
    for (auto& content : logIndex) {
        std::unique_ptr<LogEvent> targetEvent = LogEvent::CreateEvent(logGroup.GetSourceBuffer());
        targetEvent->SetTimestamp(sourceEvent.GetTimestamp(), sourceEvent.GetTimestampNanosecond()); // it is easy to forget other fields, better solution?
        targetEvent->SetContentNoCopy(StringView(splitKey.data, splitKey.size), content);
        if (mEnableLogPositionMeta) {
            auto const offset = sourceoffset + (content.data() - sourceVal.data());
            StringBuffer offsetStr = logGroup.GetSourceBuffer()->CopyString(std::to_string(offset));
            targetEvent->SetContentNoCopy(LOG_RESERVED_KEY_FILE_OFFSET, StringView(offsetStr.data, offsetStr.size));
        }
        if (sourceEvent.GetContents().size() > 1) { // copy other fields
            for (auto& kv : sourceEvent.GetContents()) {
                if (kv.first != mSplitKey && kv.first != LOG_RESERVED_KEY_FILE_OFFSET) {
                    targetEvent->SetContentNoCopy(kv.first, kv.second);
                }
            }
        }
        newEvents.emplace_back(std::move(targetEvent));
    }
}

void ProcessorSplitLogStringNative::LogSplit(const char* buffer,
                                             int32_t size,
                                             int32_t& lineFeed,
                                             std::vector<StringView>& logIndex) {
    int line_beg = 0;
    for (int i = 0; i < size; ++i) {
        if (buffer[i] == mSplitChar || buffer[i] == '\n' || i == size - 1) {
            ++lineFeed;
        }
        if (buffer[i] == mSplitChar) {
            logIndex.emplace_back(buffer + line_beg, i - line_beg);
            line_beg = i + 1;
        }
    }
    if (line_beg < size) {
        logIndex.emplace_back(buffer + line_beg, size - line_beg);
    }
}

} // namespace logtail