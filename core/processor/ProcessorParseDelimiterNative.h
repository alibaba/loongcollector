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

#include "plugin/interface/Processor.h"
#include <string>
#include <boost/regex.hpp>
#include "parser/DelimiterModeFsmParser.h"

namespace logtail {

class ProcessorParseDelimiterNative : public Processor {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const ComponentConfig& componentConfig) override;
    void Process(PipelineEventGroup& logGroup) override;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override;

private:
    bool ProcessEvent(const StringView& logPath, PipelineEventPtr& e);
    bool SplitString(const char* buffer, int32_t begIdx, int32_t endIdx, std::vector<size_t>& colBegIdxs, std::vector<size_t>& colLens);
    void AddLog(const StringView& key, const StringView& value, LogEvent& targetEvent);
    std::string mSourceKey;
    std::string mSeparator;
    std::string mRawLogTag;
    std::vector<std::string> mColumnKeys;
    bool mExtractPartialFields = false;
    bool mAutoExtend = false;
    bool mAcceptNoEnoughKeys = false;
    bool mDiscardUnmatch = false;
    bool mUploadRawLog = false;
    bool mSourceKeyOverwritten = false;
    bool mRawLogTagOverwritten = false;
    char mQuote;
    char mSeparatorChar;
    DelimiterModeFsmParser* mDelimiterModeFsmParserPtr;

    int* mLogGroupSize = nullptr;
    int* mParseFailures = nullptr;

    static const std::string s_mDiscardedFieldKey;
    CounterPtr mProcParseInSizeBytes;
    CounterPtr mProcParseOutSizeBytes;
    CounterPtr mProcDiscardRecordsTotal;
    CounterPtr mProcParseErrorTotal;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorParseDelimiterNativeUnittest;
#endif
};

} // namespace logtail