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
#include "processor/ProcessorDesensitizeNative.h"
#include "common/Constants.h"
#include "models/LogEvent.h"
#include "sdk/Common.h"
#include "plugin/instance/ProcessorInstance.h"
#include "monitor/MetricConstants.h"


namespace logtail {
const std::string ProcessorDesensitizeNative::sName = "processor_desensitize_native";

bool ProcessorDesensitizeNative::Init(const ComponentConfig& componentConfig) {
    const PipelineConfig& mConfig = componentConfig.GetConfig();

    mSensitiveWordCastOptions = mConfig.mSensitiveWordCastOptions;

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

    const LogContents& contents = sourceEvent.GetContents();

    for (auto it : mSensitiveWordCastOptions) {
        const std::string& key = it.first;
        if (!sourceEvent.HasContent(key)) {
            continue;
        }
        const auto& content = contents.find(key);
        std::string value = sourceEvent.GetContent(key).to_string();
        CastOneSensitiveWord(mSensitiveWordCastOptions[key], &value);
        mProcDesensitizeRecodesTotal->Add(1);
        StringBuffer valueBuffer = sourceEvent.GetSourceBuffer()->CopyString(value);
        sourceEvent.SetContentNoCopy(content->first, StringView(valueBuffer.data, valueBuffer.size));
    }
    return;
}

void ProcessorDesensitizeNative::CastOneSensitiveWord(const std::vector<SensitiveWordCastOption>& optionVec,
                                                      std::string* value) {
    std::string* pVal = value;
    for (size_t i = 0; i < optionVec.size(); ++i) {
        const SensitiveWordCastOption& opt = optionVec[i];
        if (!opt.mRegex || !opt.mRegex->ok()) {
            continue;
        }
        bool rst = false;

        if (opt.option == SensitiveWordCastOption::CONST_OPTION) {
            if (opt.replaceAll) {
                rst = RE2::GlobalReplace(pVal, *(opt.mRegex), opt.constValue);
            } else {
                rst = RE2::Replace(pVal, *(opt.mRegex), opt.constValue);
            }
        } else {
            re2::StringPiece srcStr(*pVal);
            size_t maxSize = pVal->size();
            size_t beginPos = 0;
            rst = true;
            std::string destStr;
            do {
                re2::StringPiece findRst;
                if (!re2::RE2::FindAndConsume(&srcStr, *(opt.mRegex), &findRst)) {
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

            } while (opt.replaceAll);

            if (rst && beginPos < pVal->size()) {
                // add ,xx
                destStr.append(pVal->substr(beginPos));
            }
            if (rst) {
                *value = destStr;
                pVal = value;
            }
        }

        // if (!rst)
        //{
        //     LOG_WARNING(sLogger, ("cast sensitive word fail", opt.constValue)(pConfig->mProjectName,
        //     pConfig->mCategory)); LogtailAlarm::GetInstance()->SendAlarm(CAST_SENSITIVE_WORD_ALARM, "cast sensitive
        //     word fail", pConfig->mProjectName, pConfig->mCategory, pConfig->mRegion);
        // }
    }
}

bool ProcessorDesensitizeNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<LogEvent>();
}

} // namespace logtail