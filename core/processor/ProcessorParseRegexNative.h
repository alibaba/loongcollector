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
#include "plugin/interface/Processor.h"
#include <vector>
#include "parser/LogParser.h" //UserDefinedFormat

namespace logtail {

class ProcessorParseRegexNative : public Processor {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const ComponentConfig& componentConfig) override;
    void Process(PipelineEventGroup& logGroup) override;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override;

private:
    void AddUserDefinedFormat(const std::string& regStr, const std::string& keys);
    /// @return false if data need to be discarded
    bool ProcessEvent(const StringView& logPath, PipelineEventPtr& e);
    bool WholeLineModeParser(LogEvent& sourceEvent, const std::string& key);
    bool RegexLogLineParser(LogEvent& sourceEvent,
                            const boost::regex& reg,
                            const std::vector<std::string>& keys,
                            const StringView& logPath);
    void AddLog(const StringView& key, const StringView& value, LogEvent& targetEvent);
    std::string mSourceKey;
    std::vector<UserDefinedFormat> mUserDefinedFormat;
    bool mDiscardUnmatch = false;
    bool mUploadRawLog = false;
    bool mSourceKeyOverwritten = false;
    bool mRawLogTagOverwritten = false;
    std::string mRawLogTag;

    int* mParseFailures = nullptr;
    int* mRegexMatchFailures = nullptr;
    int* mLogGroupSize = nullptr;

    CounterPtr mProcParseInSizeBytes;
    CounterPtr mProcParseOutSizeBytes;

    CounterPtr mProcDiscardRecordsTotal;
    CounterPtr mProcParseErrorTotal;
    CounterPtr mProcKeyCountNotMatchErrorTotal;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorParseRegexNativeUnittest;
#endif
};
} // namespace logtail