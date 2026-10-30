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

#include "plugin/processor/inner/ProcessorMergeMultilineLogNative.h"

#include <string>

#include "boost/regex.hpp"

#include "app_config/AppConfig.h"
#include "common/ParamExtractor.h"
#include "file_server/reader/LogFileReader.h"
#include "logger/Logger.h"
#include "models/LogEvent.h"
#include "monitor/metric_constants/MetricConstants.h"

namespace logtail {

const std::string ProcessorMergeMultilineLogNative::sName = "processor_merge_multiline_log_native";
const std::string ProcessorMergeMultilineLogNative::PartLogFlag = "P";

bool ProcessorMergeMultilineLogNative::Init(const Json::Value& config) {
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

    std::string mergeType;
    if (!GetMandatoryStringParam(config, "MergeType", mergeType, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    } else if (mergeType == "flag") {
        mMergeType = MergeType::BY_FLAG;
    } else if (mergeType == "json") {
        mMergeType = MergeType::BY_JSON;
    } else if (mergeType == "regex") {
        if (!mMultiline.Init(config, *mContext, sName)) {
            return false;
        }
    } else {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           "string param MergeType is not valid",
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    mMergedEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_MERGED_EVENTS_TOTAL);
    mUnmatchedEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_UNMATCHED_EVENTS_TOTAL);

    return true;
}

void ProcessorMergeMultilineLogNative::Process(PipelineEventGroup& logGroup) {
    if (logGroup.GetEvents().empty()) {
        return;
    }
    if (mMergeType == MergeType::BY_REGEX) {
        MergeLogsByRegex(logGroup);
    } else if (mMergeType == MergeType::BY_FLAG && logGroup.HasMetadata(EventGroupMetaKey::HAS_PART_LOG)) {
        // If there is no part log in the logGroup, this part of the logic does not need to be executed.
        MergeLogsByFlag(logGroup);
        logGroup.DelMetadata(EventGroupMetaKey::HAS_PART_LOG);
    } else if (mMergeType == MergeType::BY_JSON) {
        MergeLogsByJson(logGroup);
    }
}

bool ProcessorMergeMultilineLogNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    if (e.Is<LogEvent>()) {
        return true;
    }
    LOG_ERROR(
        mContext->GetLogger(),
        ("unexpected error", "some events are not supported")("processor", sName)("config", mContext->GetConfigName()));
    mContext->GetAlarm().SendAlarmWarning(SPLIT_LOG_FAIL_ALARM,
                                          "unexpected error: some events are not supported.\tprocessor: " + sName
                                              + "\tconfig: " + mContext->GetConfigName(),
                                          mContext->GetRegion(),
                                          mContext->GetProjectName(),
                                          mContext->GetConfigName(),
                                          mContext->GetLogstoreName());
    return false;
}

void ProcessorMergeMultilineLogNative::MergeLogsByFlag(PipelineEventGroup& logGroup) {
    auto& sourceEvents = logGroup.MutableEvents();
    size_t size = 0;
    std::vector<LogEvent*> events;
    bool isPartialLog = false;
    size_t begin = 0;
    size_t accumulatedSize = 0;
    size_t maxSize = mMaxMergeBlockSize > 0 ? mMaxMergeBlockSize : LogFileReader::BUFFER_SIZE;
    StringView logPath = logGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED);
    for (size_t cur = 0; cur < sourceEvents.size(); ++cur) {
        if (!IsSupportedEvent(sourceEvents[cur])) {
            if (events.empty()) {
                begin = cur;
            }
            for (size_t i = begin; i < sourceEvents.size(); ++i) {
                sourceEvents[size++] = std::move(sourceEvents[i]);
            }
            sourceEvents.resize(size);
            return;
        }
        LogEvent* sourceEvent = &sourceEvents[cur].Cast<LogEvent>();
        if (sourceEvent->Empty()) {
            continue;
        }
        events.emplace_back(sourceEvent);
        accumulatedSize += sourceEvent->GetContent(mSourceKey).size();
        if (isPartialLog) {
            // case: p p p ... p(last) notP(cur)
            if (!sourceEvent->HasContent(PartLogFlag)) {
                MergeEvents(events, false);
                sourceEvents[size++] = std::move(sourceEvents[begin]);
                begin = cur + 1;
                isPartialLog = false;
                accumulatedSize = 0;
            } else if (accumulatedSize > maxSize) {
                // 单个拆片块累积超过工作缓冲区上限(max_read_buffer_size),强制切分:
                // 输出当前块,后续拆片作为新块继续拼接。拆片间无分隔符,拼接后仍与原始行逐字节一致(数据不丢)。
                MergeEvents(events, false);
                sourceEvents[size++] = std::move(sourceEvents[begin]);
                begin = cur + 1;
                isPartialLog = false;
                accumulatedSize = 0;
                HandleMergeSizeExceeded(sourceEvents[size - 1].Cast<LogEvent>().GetContent(mSourceKey), logPath);
            }
        } else {
            if (sourceEvent->HasContent(PartLogFlag)) {
                sourceEvent->DelContent(PartLogFlag);
                isPartialLog = true;
            } else {
                MergeEvents(events, false);
                sourceEvents[size++] = std::move(sourceEvents[begin]);
                begin = cur + 1;
                accumulatedSize = 0;
            }
        }
    }
    if (isPartialLog) {
        MergeEvents(events, false);
        sourceEvents[size++] = std::move(sourceEvents[begin]);
    }
    sourceEvents.resize(size);
}

void ProcessorMergeMultilineLogNative::MergeLogsByRegex(PipelineEventGroup& logGroup) {
    /*
        Supported regex combination:
        1. start
        2. start + continue
        3. start + end
        4. continue + end
        5. end
    */
    auto& sourceEvents = logGroup.MutableEvents();
    size_t begin = 0, newSize = 0;
    std::vector<LogEvent*> events;
    std::string exception;
    bool isPartialLog = false;
    size_t accumulatedSize = 0;
    size_t maxSize = mMaxMergeBlockSize > 0 ? mMaxMergeBlockSize : LogFileReader::BUFFER_SIZE;
    StringView logPath = logGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED);

    // 累积事件并统计当前合并块大小,与超限强制切分逻辑配合。
    auto pushEvent = [&](LogEvent* e) {
        events.emplace_back(e);
        accumulatedSize += e->GetContent(mSourceKey).size();
    };
    // 强制切分:当前逻辑块累积超过工作缓冲区上限(max_read_buffer_size)时,把已累积部分合并输出,
    // 后续行仍作为同一逻辑块的延续继续累积(cut and continue)。结构可能被破坏,但数据不丢。
    auto forceSplit = [&](size_t cur) {
        MergeEvents(events, true);
        sourceEvents[newSize++] = std::move(sourceEvents[begin]);
        accumulatedSize = 0;
        begin = cur + 1;
        HandleMergeSizeExceeded(sourceEvents[newSize - 1].Cast<LogEvent>().GetContent(mSourceKey), logPath);
    };

    if (mMultiline.GetStartPatternReg() == nullptr && mMultiline.GetContinuePatternReg() == nullptr
        && mMultiline.GetEndPatternReg() != nullptr) {
        // if only end pattern is given, then it will stick to this state
        isPartialLog = true;
    }
    for (size_t cur = 0; cur < sourceEvents.size(); ++cur) {
        if (!IsSupportedEvent(sourceEvents[cur])) {
            if (events.empty()) {
                begin = cur;
            }
            for (size_t i = begin; i < sourceEvents.size(); ++i) {
                sourceEvents[newSize++] = std::move(sourceEvents[i]);
            }
            sourceEvents.resize(newSize);
            return;
        }
        LogEvent* sourceEvent = &sourceEvents[cur].Cast<LogEvent>();
        if (sourceEvent->Empty()) {
            continue;
        }
        if (!sourceEvent->HasContent(mSourceKey)) {
            if (events.empty()) {
                begin = cur;
            }
            for (size_t i = begin; i < sourceEvents.size(); ++i) {
                sourceEvents[newSize++] = std::move(sourceEvents[i]);
            }
            sourceEvents.resize(newSize);
            LOG_ERROR(mContext->GetLogger(),
                      ("unexpected error", "Some events do not have the SourceKey.")("processor", sName)(
                          "SourceKey", mSourceKey)("config", mContext->GetConfigName()));
            mContext->GetAlarm().SendAlarmWarning(
                SPLIT_LOG_FAIL_ALARM,
                "unexpected error: some events do not have the sourceKey.\tSourceKey: " + mSourceKey
                    + "\tprocessor: " + sName + "\tconfig: " + mContext->GetConfigName(),
                mContext->GetRegion(),
                mContext->GetProjectName(),
                mContext->GetConfigName(),
                mContext->GetLogstoreName());
            return;
        }
        StringView sourceVal = sourceEvent->GetContent(mSourceKey);
        if (!isPartialLog) {
            // it is impossible to enter this state if only end pattern is given
            boost::regex regex;
            if (mMultiline.GetStartPatternReg() != nullptr) {
                regex = *mMultiline.GetStartPatternReg();
            } else {
                regex = *mMultiline.GetContinuePatternReg();
            }
            if (BoostRegexSearch(sourceVal.data(), sourceVal.size(), regex, exception)) {
                pushEvent(sourceEvent);
                begin = cur;
                isPartialLog = true;
            } else if (mMultiline.GetEndPatternReg() != nullptr && mMultiline.GetStartPatternReg() == nullptr
                       && mMultiline.GetContinuePatternReg() != nullptr
                       && BoostRegexSearch(
                           sourceVal.data(), sourceVal.size(), *mMultiline.GetEndPatternReg(), exception)) {
                // case: continue + end
                // current line is matched against the end pattern rather than the continue pattern
                begin = cur;
                ADD_COUNTER(mMergedEventsTotal, 1);
                sourceEvents[newSize++] = std::move(sourceEvents[begin]);
            } else {
                HandleUnmatchLogs(sourceEvents, newSize, cur, cur, logPath);
            }
        } else {
            // case: start + continue or continue + end
            if (mMultiline.GetContinuePatternReg() != nullptr
                && BoostRegexSearch(
                    sourceVal.data(), sourceVal.size(), *mMultiline.GetContinuePatternReg(), exception)) {
                pushEvent(sourceEvent);
                if (accumulatedSize > maxSize) {
                    forceSplit(cur);
                }
                continue;
            }
            if (mMultiline.GetEndPatternReg() != nullptr) {
                // case: start + end or continue + end or end
                pushEvent(sourceEvent);
                if (mMultiline.GetContinuePatternReg() != nullptr) {
                    // current line is not matched against the continue pattern, so the end pattern will decide if
                    // the current log is a match or not
                    if (BoostRegexSearch(
                            sourceVal.data(), sourceVal.size(), *mMultiline.GetEndPatternReg(), exception)) {
                        MergeEvents(events, true);
                        sourceEvents[newSize++] = std::move(sourceEvents[begin]);
                        accumulatedSize = 0;
                    } else {
                        HandleUnmatchLogs(sourceEvents, newSize, begin, cur, logPath);
                        events.clear();
                        accumulatedSize = 0;
                    }
                    isPartialLog = false;
                } else {
                    // case: start + end or end
                    if (BoostRegexSearch(
                            sourceVal.data(), sourceVal.size(), *mMultiline.GetEndPatternReg(), exception)) {
                        MergeEvents(events, true);
                        sourceEvents[newSize++] = std::move(sourceEvents[begin]);
                        accumulatedSize = 0;
                        if (mMultiline.GetStartPatternReg() != nullptr) {
                            isPartialLog = false;
                        } else {
                            // only end pattern is given, so start another log automatically
                            begin = cur + 1;
                        }
                    }
                    // no continue pattern given, and the current line in not matched against the end pattern, so
                    // wait for the next line
                }
            } else {
                if (mMultiline.GetContinuePatternReg() == nullptr) {
                    // case: start
                    if (!BoostRegexSearch(
                            sourceVal.data(), sourceVal.size(), *mMultiline.GetStartPatternReg(), exception)) {
                        pushEvent(sourceEvent);
                    } else {
                        MergeEvents(events, true);
                        sourceEvents[newSize++] = std::move(sourceEvents[begin]);
                        accumulatedSize = 0;
                        begin = cur;
                        pushEvent(sourceEvent);
                    }
                } else {
                    // case: start + continue
                    // continue pattern is given, but current line is not matched against the continue pattern
                    MergeEvents(events, true);
                    sourceEvents[newSize++] = std::move(sourceEvents[begin]);
                    accumulatedSize = 0;
                    if (!BoostRegexSearch(
                            sourceVal.data(), sourceVal.size(), *mMultiline.GetStartPatternReg(), exception)) {
                        // when no end pattern is given, the only chance to enter unmatched state is when both start
                        // and continue pattern are given, and the current line is not matched against the start
                        // pattern
                        HandleUnmatchLogs(sourceEvents, newSize, cur, cur, logPath);
                        isPartialLog = false;
                    } else {
                        begin = cur;
                        pushEvent(sourceEvent);
                    }
                }
            }
        }
        // 强制切分:当前逻辑块仍在累积(isPartialLog)但已超过工作缓冲区上限时,切出一段(cut and continue)。
        if (isPartialLog && !events.empty() && accumulatedSize > maxSize) {
            forceSplit(cur);
        }
    }
    // when in unmatched state, the unmatched log is handled one by one, so there is no need for additional handle
    // here
    if (isPartialLog && begin < sourceEvents.size()) {
        if (mMultiline.GetEndPatternReg() == nullptr) {
            MergeEvents(events, true);
            sourceEvents[newSize++] = std::move(sourceEvents[begin]);
        } else {
            HandleUnmatchLogs(sourceEvents, newSize, begin, sourceEvents.size() - 1, logPath);
        }
    }
    sourceEvents.resize(newSize);
}

void ProcessorMergeMultilineLogNative::MergeEvents(std::vector<LogEvent*>& logEvents, bool insertLineBreak) {
    if (logEvents.size() == 0) {
        return;
    }
    ADD_COUNTER(mMergedEventsTotal, logEvents.size());
    if (logEvents.size() == 1) {
        logEvents.clear();
        return;
    }
    LogEvent* targetEvent = logEvents[0];
    StringView targetValue = targetEvent->GetContent(mSourceKey);
    char* begin = const_cast<char*>(targetValue.data());
    char* end = begin + targetValue.size();

    for (size_t index = 1; index < logEvents.size(); ++index) {
        if (insertLineBreak) {
            *end = '\n';
            ++end;
        }
        StringView curValue = logEvents[index]->GetContent(mSourceKey);
        memmove(end, curValue.data(), curValue.size());
        end += curValue.size();
    }
    targetEvent->SetContentNoCopy(mSourceKey, StringView(begin, end - begin));
    logEvents.clear();
}

void ProcessorMergeMultilineLogNative::HandleUnmatchLogs(
    std::vector<PipelineEventPtr>& logEvents, size_t& newSize, size_t begin, size_t end, StringView logPath) {
    ADD_COUNTER(mUnmatchedEventsTotal, end - begin + 1);
    if (mMultiline.mUnmatchedContentTreatment == MultilineOptions::UnmatchedContentTreatment::DISCARD
        && mMultiline.mIgnoringUnmatchWarning) {
        return;
    }
    for (size_t i = begin; i <= end; i++) {
        if (!mMultiline.mIgnoringUnmatchWarning && AlarmManager::GetInstance()->IsLowLevelAlarmValid()) {
            StringView sourceVal = logEvents[i].Cast<LogEvent>().GetContent(mSourceKey);
            LOG_WARNING(
                GetContext().GetLogger(),
                ("unmatched log line", "please check regex")(
                    "action", UnmatchedContentTreatmentToString(mMultiline.mUnmatchedContentTreatment))(
                    "first 1KB", sourceVal.substr(0, 1024).to_string())("filepath", logPath.to_string())(
                    "processor", sName)("config", GetContext().GetConfigName())("log bytes", sourceVal.size() + 1));
            GetContext().GetAlarm().SendAlarmWarning(
                SPLIT_LOG_FAIL_ALARM,
                "unmatched log line, first 1KB:" + sourceVal.substr(0, 1024).to_string() + "\taction: "
                    + UnmatchedContentTreatmentToString(mMultiline.mUnmatchedContentTreatment) + "\tfilepath: "
                    + logPath.to_string() + "\tprocessor: " + sName + "\tconfig: " + GetContext().GetConfigName(),
                GetContext().GetRegion(),
                GetContext().GetProjectName(),
                GetContext().GetConfigName(),
                GetContext().GetLogstoreName());
        }
        if (mMultiline.mUnmatchedContentTreatment == MultilineOptions::UnmatchedContentTreatment::SINGLE_LINE) {
            logEvents[newSize++] = std::move(logEvents[i]);
        }
    }
}

void ProcessorMergeMultilineLogNative::HandleMergeSizeExceeded(StringView mergedContent, StringView logPath) {
    if (!AlarmManager::GetInstance()->IsLowLevelAlarmValid()) {
        return;
    }
    LOG_WARNING(mContext->GetLogger(),
                ("merged log block exceeds max_read_buffer_size, forced to split",
                 "")("first 1KB", mergedContent.substr(0, 1024).to_string())("filepath", logPath.to_string())(
                    "processor", sName)("config", mContext->GetConfigName()));
    mContext->GetAlarm().SendAlarmWarning(
        SPLIT_LOG_FAIL_ALARM,
        "merged log block exceeds max_read_buffer_size and forced to split. processor: " + sName
            + " config: " + mContext->GetConfigName(),
        mContext->GetRegion(),
        mContext->GetProjectName(),
        mContext->GetConfigName(),
        mContext->GetLogstoreName());
}

void ProcessorMergeMultilineLogNative::MergeLogsByJson(PipelineEventGroup& logGroup) {
    auto& sourceEvents = logGroup.MutableEvents();
    size_t newSize = 0;
    std::vector<LogEvent*> events;
    size_t begin = 0;
    int braceDepth = 0;
    bool inQuote = false;
    bool pendingEscape = false;
    size_t accumulatedSize = 0;
    size_t maxSize = mMaxMergeBlockSize > 0 ? mMaxMergeBlockSize : LogFileReader::BUFFER_SIZE;
    StringView logPath = logGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED);

    for (size_t cur = 0; cur < sourceEvents.size(); ++cur) {
        if (!IsSupportedEvent(sourceEvents[cur])) {
            if (events.empty()) {
                begin = cur;
            }
            for (size_t i = begin; i < sourceEvents.size(); ++i) {
                sourceEvents[newSize++] = std::move(sourceEvents[i]);
            }
            sourceEvents.resize(newSize);
            return;
        }
        LogEvent* sourceEvent = &sourceEvents[cur].Cast<LogEvent>();
        if (sourceEvent->Empty()) {
            continue;
        }

        StringView content = sourceEvent->GetContent(mSourceKey);

        for (size_t i = 0; i < content.size(); ++i) {
            if (pendingEscape) {
                pendingEscape = false;
                continue;
            }
            char c = content[i];
            switch (c) {
                case '\\':
                    if (inQuote) {
                        if (i + 1 < content.size()) {
                            ++i;
                        } else {
                            pendingEscape = true;
                        }
                    }
                    break;
                case '"':
                    inQuote = !inQuote;
                    break;
                case '{':
                    if (!inQuote) {
                        braceDepth++;
                    }
                    break;
                case '}':
                    if (!inQuote) {
                        braceDepth--;
                    }
                    break;
                default:
                    break;
            }
        }

        if (events.empty()) {
            begin = cur;
        }
        events.emplace_back(sourceEvent);
        accumulatedSize += content.size();

        if (braceDepth <= 0) {
            if (braceDepth < 0) {
                LOG_DEBUG(
                    mContext->GetLogger(),
                    ("JSON brace depth went negative, possible malformed input", "")("braceDepth", braceDepth)(
                        "filepath", logPath.to_string())("processor", sName)("config", mContext->GetConfigName()));
            }
            MergeEvents(events, true);
            sourceEvents[newSize++] = std::move(sourceEvents[begin]);
            events.clear();
            braceDepth = 0;
            inQuote = false;
            pendingEscape = false;
            accumulatedSize = 0;
        } else if (accumulatedSize > maxSize) {
            MergeEvents(events, true);
            sourceEvents[newSize++] = std::move(sourceEvents[begin]);
            events.clear();
            braceDepth = 0;
            inQuote = false;
            pendingEscape = false;
            accumulatedSize = 0;
            HandleMergeSizeExceeded(sourceEvents[newSize - 1].Cast<LogEvent>().GetContent(mSourceKey), logPath);
        }
    }

    if (!events.empty()) {
        MergeEvents(events, true);
        sourceEvents[newSize++] = std::move(sourceEvents[begin]);
    }
    sourceEvents.resize(newSize);
}

} // namespace logtail
