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

#include "plugin/processor/ProcessorParseDelimiterNative.h"

#include "common/ParamExtractor.h"
#include "models/LogEvent.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "pipeline/plugin/instance/ProcessorInstance.h"

namespace logtail {

const std::string ProcessorParseDelimiterNative::sName = "processor_parse_delimiter_native";

const std::string ProcessorParseDelimiterNative::s_mDiscardedFieldKey = "_";

bool ProcessorParseDelimiterNative::Init(const Json::Value& config) {
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

    // Separator
    if (!GetMandatoryStringParam(config, "Separator", mSeparator, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    if (mSeparator.size() > 4) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           "mandatory string param Separator has more than 4 chars",
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    // Compatible with old logic.
    if (mSeparator == "\\t") {
        mSeparator = '\t';
    }
    mSeparatorChar = mSeparator[0];

    // Quote
    std::string quoteStr;
    bool res = GetOptionalStringParam(config, "Quote", quoteStr, errorMsg);
    if (mSeparator.size() == 1) {
        if (!res) {
            PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                                  mContext->GetAlarm(),
                                  errorMsg,
                                  "\"",
                                  sName,
                                  mContext->GetConfigName(),
                                  mContext->GetProjectName(),
                                  mContext->GetLogstoreName(),
                                  mContext->GetRegion());
        } else if (quoteStr.size() > 1) {
            PARAM_ERROR_RETURN(mContext->GetLogger(),
                               mContext->GetAlarm(),
                               "string param Quote is not a single char",
                               sName,
                               mContext->GetConfigName(),
                               mContext->GetProjectName(),
                               mContext->GetLogstoreName(),
                               mContext->GetRegion());
        } else if (!quoteStr.empty()) {
            mQuote = quoteStr[0];
        }
    } else if (!quoteStr.empty()) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             "string param Quote is not allowed when param Separator is not a single char",
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }

    mDelimiterModeFsmParserPtr.reset(new DelimiterModeFsmParser(mQuote, mSeparatorChar));

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
    for (const auto& key : mKeys) {
        if (key == mSourceKey) {
            mSourceKeyOverwritten = true;
        }
    }

    // AllowingShortenedFields
    if (!GetOptionalBoolParam(config, "AllowingShortenedFields", mAllowingShortenedFields, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mAllowingShortenedFields,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    // OverflowedFieldsTreatment
    std::string overflowedFieldsTreatment;
    if (!GetOptionalStringParam(config, "OverflowedFieldsTreatment", overflowedFieldsTreatment, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              "extend",
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    } else if (overflowedFieldsTreatment == "keep") {
        mOverflowedFieldsTreatment = OverflowedFieldsTreatment::KEEP;
    } else if (overflowedFieldsTreatment == "discard") {
        mOverflowedFieldsTreatment = OverflowedFieldsTreatment::DISCARD;
    } else if (!overflowedFieldsTreatment.empty() && overflowedFieldsTreatment != "extend") {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              "string param OverflowedFieldsTreatment is not valid",
                              "extend",
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    // ExtractingPartialFields
    // TODO: currently OverflowedFieldsTreatment additionally controls ExtractingPartialFields, which should be
    // separated in the future.
    mExtractingPartialFields = mOverflowedFieldsTreatment == OverflowedFieldsTreatment::DISCARD;

    if (!mCommonParserOptions.Init(config, *mContext, sName)) {
        return false;
    }

    mParseFailures = &(GetContext().GetProcessProfile().parseFailures);
    mLogGroupSize = &(GetContext().GetProcessProfile().logGroupSize);

    mDiscardedEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_DISCARDED_EVENTS_TOTAL);
    mOutFailedEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_FAILED_EVENTS_TOTAL);
    mOutKeyNotFoundEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_KEY_NOT_FOUND_EVENTS_TOTAL);
    mOutSuccessfulEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_SUCCESSFUL_EVENTS_TOTAL);

    return true;
}

void ProcessorParseDelimiterNative::Process(PipelineEventGroup& logGroup) {
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

bool ProcessorParseDelimiterNative::ProcessEvent(const StringView& logPath, PipelineEventPtr& e) {
    if (!IsSupportedEvent(e)) {
        mOutFailedEventsTotal->Add(1);
        return true;
    }
    LogEvent& sourceEvent = e.Cast<LogEvent>();
    if (!sourceEvent.HasContent(mSourceKey)) {
        mOutKeyNotFoundEventsTotal->Add(1);
        return true;
    }
    StringView buffer = sourceEvent.GetContent(mSourceKey);

    int32_t endIdx = buffer.size();
    if (endIdx == 0) {
        mOutFailedEventsTotal->Add(1);
        return true;
    }

    for (int32_t i = endIdx - 1; i >= 0; --i) {
        if (buffer.data()[i] == ' ' || '\r' == buffer.data()[i])
            endIdx = i;
        else
            break;
    }
    int32_t begIdx = 0;
    for (int32_t i = 0; i < endIdx; ++i) {
        if (buffer.data()[i] == ' ')
            begIdx = i + 1;
        else
            break;
    }
    if (begIdx >= endIdx) {
        mOutFailedEventsTotal->Add(1);
        return true;
    }
        
    size_t reserveSize
        = mOverflowedFieldsTreatment == OverflowedFieldsTreatment::EXTEND ? (mKeys.size() + 10) : (mKeys.size() + 1);
    std::vector<StringView> columnValues;
    std::vector<size_t> colBegIdxs;
    std::vector<size_t> colLens;
    bool parseSuccess = false;
    size_t parsedColCount = 0;
    bool useQuote = (mSeparator.size() == 1) && (mQuote != mSeparatorChar);
    if (mKeys.size() > 0) {
        if (useQuote) {
            columnValues.reserve(reserveSize);
            parseSuccess
                = mDelimiterModeFsmParserPtr->ParseDelimiterLine(buffer, begIdx, endIdx, columnValues, sourceEvent);
            // handle auto extend
            if (!(mOverflowedFieldsTreatment == OverflowedFieldsTreatment::EXTEND)
                && columnValues.size() > mKeys.size()) {
                int requiredLen = 0;
                for (size_t i = mKeys.size(); i < columnValues.size(); ++i) {
                    requiredLen += 1 + columnValues[i].size();
                }
                StringBuffer sb = sourceEvent.GetSourceBuffer()->AllocateStringBuffer(requiredLen);
                char* extraFields = sb.data;
                for (size_t i = mKeys.size(); i < columnValues.size(); ++i) {
                    extraFields[0] = mSeparatorChar;
                    extraFields++;
                    memcpy(extraFields, columnValues[i].data(), columnValues[i].size());
                    extraFields += columnValues[i].size();
                }
                // remove extra fields
                columnValues.resize(mKeys.size());
                columnValues.push_back(StringView(sb.data, requiredLen));
            }
            parsedColCount = columnValues.size();
        } else {
            colBegIdxs.reserve(reserveSize);
            colLens.reserve(reserveSize);
            parseSuccess = SplitString(buffer.data(), begIdx, endIdx, colBegIdxs, colLens);
            parsedColCount = colBegIdxs.size();
        }

        if (parseSuccess) {
            if (parsedColCount <= 0 || (!mAllowingShortenedFields && parsedColCount < mKeys.size())) {
                LOG_WARNING(
                    sLogger,
                    ("parse delimiter log fail, keys count unmatch "
                     "columns count, parsed",
                     parsedColCount)("required", mKeys.size())("log", buffer)("project", GetContext().GetProjectName())(
                        "logstore", GetContext().GetLogstoreName())("file", logPath));
                GetContext().GetAlarm().SendAlarm(PARSE_LOG_FAIL_ALARM,
                                                  std::string("keys count unmatch columns count :")
                                                      + ToString(parsedColCount) + ", required:"
                                                      + ToString(mKeys.size()) + ", logs:" + buffer.to_string(),
                                                  GetContext().GetProjectName(),
                                                  GetContext().GetLogstoreName(),
                                                  GetContext().GetRegion());
                ++(*mParseFailures);
                parseSuccess = false;
            }
        } else {
            LogtailAlarm::GetInstance()->SendAlarm(PARSE_LOG_FAIL_ALARM,
                                                   std::string("parse delimiter log fail")
                                                       + ", logs:" + buffer.to_string(),
                                                   GetContext().GetProjectName(),
                                                   GetContext().GetLogstoreName(),
                                                   GetContext().GetRegion());
            ++(*mParseFailures);
            parseSuccess = false;
        }
    } else {
        LogtailAlarm::GetInstance()->SendAlarm(PARSE_LOG_FAIL_ALARM,
                                               "no column keys defined",
                                               GetContext().GetProjectName(),
                                               GetContext().GetLogstoreName(),
                                               GetContext().GetRegion());
        LOG_WARNING(sLogger,
                    ("parse delimiter log fail", "no column keys defined")("project", GetContext().GetProjectName())(
                        "logstore", GetContext().GetLogstoreName())("file", logPath));
        ++(*mParseFailures);
        parseSuccess = false;
    }

    if (parseSuccess) {
        for (uint32_t idx = 0; idx < parsedColCount; idx++) {
            if (mKeys.size() > idx) {
                if (mExtractingPartialFields && mKeys[idx] == s_mDiscardedFieldKey) {
                    continue;
                }
                AddLog(mKeys[idx],
                       useQuote ? columnValues[idx] : StringView(buffer.data() + colBegIdxs[idx], colLens[idx]),
                       sourceEvent);
            } else {
                if (mExtractingPartialFields) {
                    continue;
                }
                std::string key = "__column" + ToString(idx) + "__";
                StringBuffer sb = sourceEvent.GetSourceBuffer()->CopyString(key);
                AddLog(StringView(sb.data, sb.size),
                       useQuote ? columnValues[idx] : StringView(buffer.data() + colBegIdxs[idx], colLens[idx]),
                       sourceEvent);
            }
        }
        mOutSuccessfulEventsTotal->Add(1);
    } else {
        mOutFailedEventsTotal->Add(1);
    }

    if (!parseSuccess || !mSourceKeyOverwritten) {
        sourceEvent.DelContent(mSourceKey);
    }
    if (mCommonParserOptions.ShouldAddSourceContent(parseSuccess)) {
        AddLog(mCommonParserOptions.mRenamedSourceKey, buffer, sourceEvent, false);
    }
    if (mCommonParserOptions.ShouldAddLegacyUnmatchedRawLog(parseSuccess)) {
        AddLog(mCommonParserOptions.legacyUnmatchedRawLogKey, buffer, sourceEvent, false);
    }
    if (mCommonParserOptions.ShouldEraseEvent(parseSuccess, sourceEvent)) {
        mDiscardedEventsTotal->Add(1);
        return false;
    }
    return true;
}

bool ProcessorParseDelimiterNative::SplitString(
    const char* buffer, int32_t begIdx, int32_t endIdx, std::vector<size_t>& colBegIdxs, std::vector<size_t>& colLens) {
    if (endIdx <= begIdx || mSeparator.size() == 0 || mKeys.size() == 0)
        return false;
    size_t size = endIdx - begIdx;
    size_t d_size = mSeparator.size();
    if (d_size == 0 || d_size > size) {
        colBegIdxs.push_back(begIdx);
        colLens.push_back(size);
        return true;
    }
    size_t pos = begIdx;
    size_t top = endIdx - d_size;
    while (pos <= top) {
        const char* pch = std::search(buffer + pos, buffer + endIdx, mSeparator.begin(), mSeparator.end());
        size_t pos2;
        // if not found, pos2 = endIdx
        if (pch == buffer + endIdx) {
            pos2 = endIdx;
        } else {
            pos2 = pch - buffer;
        }
        if (pos2 != pos) {
            colBegIdxs.push_back(pos);
            colLens.push_back(pos2 - pos);
        } else {
            colBegIdxs.push_back(pos);
            colLens.push_back(0);
        }
        if (pos2 == (size_t)endIdx)
            return true;
        pos = pos2 + d_size;
        if (colLens.size() >= mKeys.size() && !(mOverflowedFieldsTreatment == OverflowedFieldsTreatment::EXTEND)) {
            colBegIdxs.push_back(pos2);
            colLens.push_back(endIdx - pos2);
            return true;
        }
    }
    if (pos <= (size_t)endIdx) {
        colBegIdxs.push_back(pos);
        colLens.push_back(endIdx - pos);
    }
    return true;
}

void ProcessorParseDelimiterNative::AddLog(const StringView& key,
                                           const StringView& value,
                                           LogEvent& targetEvent,
                                           bool overwritten) {
    if (!overwritten && targetEvent.HasContent(key)) {
        return;
    }
    targetEvent.SetContentNoCopy(key, value);
    *mLogGroupSize += key.size() + value.size() + 5;
}

bool ProcessorParseDelimiterNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<LogEvent>();
}

} // namespace logtail