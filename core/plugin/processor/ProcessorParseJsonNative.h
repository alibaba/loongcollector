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
#pragma once

#include <rapidjson/document.h>

#include "models/LogEvent.h"
#include "pipeline/plugin/interface/Processor.h"
#include "plugin/processor/CommonParserOptions.h"

namespace logtail {

class ProcessorParseJsonNative : public Processor {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config) override;
    void Process(PipelineEventGroup& logGroup) override;

    // Source field name.
    std::string mSourceKey;
    CommonParserOptions mCommonParserOptions;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override;

private:
    bool JsonLogLineParser(LogEvent& sourceEvent, const StringView& logPath, PipelineEventPtr& e, bool& sourceKeyOverwritten);
    void AddLog(const StringView& key, const StringView& value, LogEvent& targetEvent, bool overwritten = true);
    bool ProcessEvent(const StringView& logPath, PipelineEventPtr& e);
    static std::string RapidjsonValueToString(const rapidjson::Value& value);

    int* mParseFailures = nullptr;
    int* mLogGroupSize = nullptr;

    CounterPtr mDiscardedEventsTotal;
    CounterPtr mOutFailedEventsTotal;
    CounterPtr mOutKeyNotFoundEventsTotal;
    CounterPtr mOutSuccessfulEventsTotal;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorParseJsonNativeUnittest;
#endif
};

} // namespace logtail
