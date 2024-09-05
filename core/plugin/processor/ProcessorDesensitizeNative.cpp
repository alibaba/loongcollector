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
#include "plugin/processor/ProcessorDesensitizeNative.h"

#include "common/Constants.h"
#include "common/ParamExtractor.h"
#include "models/LogEvent.h"
#include "monitor/MetricConstants.h"
#include "pipeline/plugin/instance/ProcessorInstance.h"
#include "sdk/Common.h"

namespace logtail {

const std::string ProcessorDesensitizeNative::sName = "processor_desensitize_native";

bool ProcessorDesensitizeNative::Init(const Json::Value& config) {
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

    // Method
    std::string method;
    if (!GetMandatoryStringParam(config, "Method", method, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    if (method == "const") {
        mMethod = DesensitizeMethod::CONST_OPTION;
    } else if (method == "md5") {
        mMethod = DesensitizeMethod::MD5_OPTION;
    } else {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           "string param Method is not valid",
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    // ReplacingString
    if (mMethod == DesensitizeMethod::CONST_OPTION) {
        if (!GetMandatoryStringParam(config, "ReplacingString", mReplacingString, errorMsg)) {
            PARAM_ERROR_RETURN(mContext->GetLogger(),
                               mContext->GetAlarm(),
                               errorMsg,
                               sName,
                               mContext->GetConfigName(),
                               mContext->GetProjectName(),
                               mContext->GetLogstoreName(),
                               mContext->GetRegion());
        }
    }
    mReplacingString = std::string("\\1") + mReplacingString;

    // ContentPatternBeforeReplacedString
    if (!GetMandatoryStringParam(
            config, "ContentPatternBeforeReplacedString", mContentPatternBeforeReplacedString, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    // ReplacedContentPattern
    if (!GetMandatoryStringParam(config, "ReplacedContentPattern", mReplacedContentPattern, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    std::string regexStr = std::string("(") + mContentPatternBeforeReplacedString + ")" + mReplacedContentPattern;
    mRegex.reset(new re2::RE2(regexStr));
    if (!mRegex->ok()) {
        errorMsg = mRegex->error();
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           "param ContentPatternBeforeReplacedString or ReplacedContentPattern is not a valid regex: "
                               + errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    // ReplacingAll
    if (!GetOptionalBoolParam(config, "ReplacingAll", mReplacingAll, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mReplacingAll,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    mProcDesensitizeRecodesTotal = GetMetricsRecordRef().CreateCounter(METRIC_PROC_DESENSITIZE_RECORDS_TOTAL);

    return true;
}

void ProcessorDesensitizeNative::Process(PipelineEventGroup& logGroup) {
    if (logGroup.GetEvents().empty()) {
        return;
    }

    EventsContainer& events = logGroup.MutableEvents();

    for (auto it = events.begin(); it != events.end();) {
        ProcessEvent(*it);
        ++it;
    }
}

void ProcessorDesensitizeNative::ProcessEvent(PipelineEventPtr& e) {
    if (!IsSupportedEvent(e)) {
        return;
    }

    auto& sourceEvent = e.Cast<LogEvent>();

    // Traverse all fields and desensitize sensitive fields.
    for (auto& item : sourceEvent) {
        // Only perform desensitization processing on specified fields.
        if (item.first != mSourceKey) {
            continue;
        }
        // Only perform desensitization processing on non-empty fields.
        if (item.second.empty()) {
            continue;
        }
        std::string value = item.second.to_string();
        CastOneSensitiveWord(&value);
        mProcDesensitizeRecodesTotal->Add(1);
        StringBuffer valueBuffer = sourceEvent.GetSourceBuffer()->CopyString(value);
        sourceEvent.SetContentNoCopy(item.first, StringView(valueBuffer.data, valueBuffer.size));
    }
}

void ProcessorDesensitizeNative::CastOneSensitiveWord(std::string* value) {
    std::string* pVal = value;
    bool rst = false;

    if (mMethod == DesensitizeMethod::CONST_OPTION) {
        if (mReplacingAll) {
            rst = RE2::GlobalReplace(pVal, *mRegex, mReplacingString);
        } else {
            rst = RE2::Replace(pVal, *mRegex, mReplacingString);
        }
    } else {
        re2::StringPiece srcStr(*pVal);
        size_t maxSize = pVal->size();
        size_t beginPos = 0;
        rst = true;
        std::string destStr;
        do {
            re2::StringPiece findRst;
            if (!re2::RE2::FindAndConsume(&srcStr, *mRegex, &findRst)) {
                if (beginPos == (size_t)0) {
                    rst = false;
                }
                break;
            }
            // like  xxxx, psw=123abc,xx
            size_t beginOffset = findRst.data() + findRst.size() - pVal->data();
            size_t endOffset = srcStr.empty() ? maxSize : srcStr.data() - pVal->data();
            if (beginOffset < beginPos || endOffset <= beginPos || endOffset > maxSize) {
                rst = false;
                break;
            }
            // add : xxxx, psw
            destStr.append(pVal->substr(beginPos, beginOffset - beginPos));
            // md5: 123abc
            destStr.append(sdk::CalcMD5(pVal->substr(beginOffset, endOffset - beginOffset)));
            beginPos = endOffset;
            // refine for  : xxxx. psw=123abc
            if (endOffset >= maxSize) {
                break;
            }
        } while (mReplacingAll);

        if (rst && beginPos < pVal->size()) {
            // add ,xx
            destStr.append(pVal->substr(beginPos));
        }
        if (rst) {
            *value = destStr;
            pVal = value;
        }
    }
}

bool ProcessorDesensitizeNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<LogEvent>();
}

} // namespace logtail