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

#include <boost/regex.hpp>

#include <vector>

#include "models/LogEvent.h"
#include "pipeline/plugin/interface/Processor.h"
#include "plugin/processor/CommonParserOptions.h"

namespace logtail {

class ProcessorParseRegexNative : public Processor {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config) override;
    void Process(PipelineEventGroup& logGroup) override;

    // Source field name.
    std::string mSourceKey;
    // Regular expression.
    std::string mRegex;
    // Extracted field list.
    std::vector<std::string> mKeys;
    CommonParserOptions mCommonParserOptions;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override;

private:
    /// @return false if data need to be discarded
    bool ProcessEvent(const StringView& logPath, PipelineEventPtr& e);
    bool WholeLineModeParser(LogEvent& sourceEvent, const std::string& key);
    bool RegexLogLineParser(LogEvent& sourceEvent,
                            const boost::regex& reg,
                            const std::vector<std::string>& keys,
                            const StringView& logPath);
    void AddLog(const StringView& key, const StringView& value, LogEvent& targetEvent, bool overwritten = true);

    bool mSourceKeyOverwritten = false;
    bool mIsWholeLineMode = false;
    boost::regex mReg;

    int* mParseFailures = nullptr;
    int* mRegexMatchFailures = nullptr;
    int* mLogGroupSize = nullptr;

    CounterPtr mDiscardedEventsTotal;
    CounterPtr mOutFailedEventsTotal;
    CounterPtr mOutKeyNotFoundEventsTotal;
    CounterPtr mOutSuccessfulEventsTotal;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorParseRegexNativeUnittest;
#endif
};

} // namespace logtail
