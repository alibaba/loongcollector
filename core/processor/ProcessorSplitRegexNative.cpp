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

#include "processor/ProcessorSplitRegexNative.h"
#include "common/Constants.h"
#include "reader/LogFileReader.h" //SplitState
#include "models/LogEvent.h"
#include "logger/Logger.h"

namespace logtail {

bool ProcessorSplitRegexNative::Init(const ComponentConfig& config) {
    mSplitKey = DEFAULT_CONTENT_KEY;
    mLogBeginReg = config.mLogBeginReg;
    if (mLogBeginReg.empty() == false && mLogBeginReg != ".*") {
        mLogBeginRegPtr.reset(new boost::regex(mLogBeginReg));
    }
    mDiscardUnmatch = config.mDiscardUnmatch;
    mEnableLogPositionMeta = config.mAdvancedConfig.mEnableLogPositionMeta;
    mFeedLines = &(GetContext().GetProcessProfile().feedLines);
    mSplitLines = &(GetContext().GetProcessProfile().splitLines);
    return true;
}

void ProcessorSplitRegexNative::Process(PipelineEventGroup& logGroup) {
    if (logGroup.GetEvents().empty()) {
        return;
    }
    EventsContainer newEvents;
    const StringView& logPath = logGroup.GetMetadata(EVENT_META_LOG_FILE_PATH_RESOLVED);
    for (const PipelineEventPtr& e : logGroup.GetEvents()) {
        ProcessEvent(logGroup, logPath, e, newEvents);
    }
    *mSplitLines = newEvents.size();
    logGroup.SwapEvents(newEvents);

    return;
}

bool ProcessorSplitRegexNative::IsSupportedEvent(const PipelineEventPtr& e) {
    return e.Is<LogEvent>();
}

void ProcessorSplitRegexNative::ProcessEvent(PipelineEventGroup& logGroup,
                                             const StringView& logPath,
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
    std::vector<StringView> discardIndex; // used to send warning
    int feedLines = 0;
    bool splitSuccess = LogSplit(sourceVal.data(), sourceVal.size(), feedLines, logIndex, discardIndex, logPath);
    *mFeedLines += feedLines;

    if (AppConfig::GetInstance()->IsLogParseAlarmValid() && LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
        if (!splitSuccess) { // warning if unsplittable
            GetContext().GetAlarm().SendAlarm(SPLIT_LOG_FAIL_ALARM,
                                              "split log lines fail, please check log_begin_regex, file:"
                                                  + logPath.to_string()
                                                  + ", logs:" + sourceVal.substr(0, 1024).to_string(),
                                              GetContext().GetProjectName(),
                                              GetContext().GetLogstoreName(),
                                              GetContext().GetRegion());
            LOG_ERROR(GetContext().GetLogger(),
                      ("split log lines fail", "please check log_begin_regex")("file_name", logPath)(
                          "log bytes", sourceVal.size() + 1)("first 1KB log", sourceVal.substr(0, 1024).to_string()));
        }
        for (auto& discardData : discardIndex) { // warning if data loss
            GetContext().GetAlarm().SendAlarm(SPLIT_LOG_FAIL_ALARM,
                                              "split log lines discard data, file:" + logPath.to_string()
                                                  + ", logs:" + discardData.substr(0, 1024).to_string(),
                                              GetContext().GetProjectName(),
                                              GetContext().GetLogstoreName(),
                                              GetContext().GetRegion());
            LOG_WARNING(
                GetContext().GetLogger(),
                ("split log lines discard data", "please check log_begin_regex")("file_name", logPath)(
                    "log bytes", sourceVal.size() + 1)("first 1KB log", discardData.substr(0, 1024).to_string()));
        }
    }
    if (splitSuccess) {
        long sourceoffset = 0L;
        if (sourceEvent.HasContent(EVENT_META_LOG_FILE_OFFSET)) {
            sourceoffset = atol(sourceEvent.GetContent(EVENT_META_LOG_FILE_OFFSET).data()); // use safer method
        }
        StringBuffer splitKey = logGroup.GetSourceBuffer()->CopyString(mSplitKey);
        for (auto& content : logIndex) {
            std::unique_ptr<LogEvent> targetEvent = LogEvent::CreateEvent(logGroup.GetSourceBuffer());
            targetEvent->SetTimestamp(sourceEvent.GetTimestamp()); // it is easy to forget other fields, better solution?
            targetEvent->SetContentNoCopy(StringView(splitKey.data, splitKey.size), content);
            if (mEnableLogPositionMeta) {
                auto const offset = sourceoffset + (content.data() - sourceVal.data());
                StringBuffer offsetStr = logGroup.GetSourceBuffer()->CopyString(std::to_string(offset));
                targetEvent->SetContentNoCopy(EVENT_META_LOG_FILE_OFFSET, StringView(offsetStr.data, offsetStr.size));
            }
            if (sourceEvent.GetContents().size() > 1) { // copy other fields
                for (auto& kv : sourceEvent.GetContents()) {
                    if (kv.first != mSplitKey && kv.first != EVENT_META_LOG_FILE_OFFSET) {
                        targetEvent->SetContentNoCopy(kv.first, kv.second);
                    }
                }
            }
            newEvents.emplace_back(std::move(targetEvent));
        }
    } else {
        newEvents.emplace_back(e);
    }
}

bool ProcessorSplitRegexNative::LogSplit(const char* buffer,
                                         int32_t size,
                                         int32_t& lineFeed,
                                         std::vector<StringView>& logIndex,
                                         std::vector<StringView>& discardIndex,
                                         const StringView& logPath) {
    SplitState state = SPLIT_UNMATCH;
    int multiBegIndex = 0;
    int begIndex = 0;
    int endIndex = 0;
    bool anyMatched = false;
    lineFeed = 0;
    std::string exception;
    while (endIndex < size) {
        if (buffer[endIndex] == '\n' || endIndex == size - 1) {
            ++lineFeed;
            exception.clear();
            if (mLogBeginRegPtr == NULL
                || BoostRegexMatch(buffer + begIndex, endIndex - begIndex, *mLogBeginRegPtr, exception)) {
                anyMatched = true;
                if (multiBegIndex < begIndex) {
                    if (state == SPLIT_UNMATCH && mDiscardUnmatch) {
                        discardIndex.emplace_back(buffer + multiBegIndex, begIndex - 1 - multiBegIndex);
                    } else {
                        logIndex.emplace_back(buffer + multiBegIndex, begIndex - 1 - multiBegIndex);
                    }
                    multiBegIndex = begIndex;
                }
                state = SPLIT_BEGIN;
            } else {
                if (state == SPLIT_BEGIN || state == SPLIT_CONTINUE) {
                    state = SPLIT_CONTINUE;
                }
                if (!exception.empty() && AppConfig::GetInstance()->IsLogParseAlarmValid()) {
                    if (GetContext().GetAlarm().IsLowLevelAlarmValid()) {
                        LOG_ERROR(GetContext().GetLogger(),
                                  ("regex_match in LogSplit fail, exception", exception)("project",
                                                                                         GetContext().GetProjectName())(
                                      "logstore", GetContext().GetLogstoreName())("file", logPath));
                        GetContext().GetAlarm().SendAlarm(REGEX_MATCH_ALARM,
                                                          "regex_match in LogSplit fail:" + exception + ", file"
                                                              + logPath.to_string(),
                                                          GetContext().GetProjectName(),
                                                          GetContext().GetLogstoreName(),
                                                          GetContext().GetRegion());
                    }
                }
            }
            begIndex = endIndex + 1;
        }
        endIndex++;
    }
    if (state == SPLIT_UNMATCH && mDiscardUnmatch) {
        discardIndex.emplace_back(buffer + multiBegIndex, begIndex - multiBegIndex);
    } else {
        logIndex.emplace_back(buffer + multiBegIndex, begIndex - multiBegIndex);
    }
    if (!exception.empty()) {
        if (AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                LOG_ERROR(GetContext().GetLogger(),
                          ("regex_match in LogSplit fail, exception", exception)("project", GetContext().GetProjectName())(
                              "logstore", GetContext().GetLogstoreName())("file", logPath));
            }
            GetContext().GetAlarm().SendAlarm(REGEX_MATCH_ALARM,
                                              "regex_match in LogSplit fail:" + exception,
                                              GetContext().GetProjectName(),
                                              GetContext().GetLogstoreName(),
                                              GetContext().GetRegion());
        }
    }
    return anyMatched;
}
} // namespace logtail