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

#include <memory>

#include "models/LogEvent.h"
#include "parser/DelimiterModeFsmParser.h"
#include "pipeline/plugin/interface/Processor.h"
#include "plugin/processor/CommonParserOptions.h"

namespace logtail {

class ProcessorParseDelimiterNative : public Processor {
public:
    enum class OverflowedFieldsTreatment { EXTEND, KEEP, DISCARD };

    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config) override;
    void Process(PipelineEventGroup& logGroup) override;

    // Required: source field name.
    std::string mSourceKey;
    // Required: Separator
    std::string mSeparator;
    // Quotation mark
    char mQuote = '"';
    // Required: List of extracted fields.
    std::vector<std::string> mKeys;
    // Whether the number of fields allowed to be extracted is less than the number of Keys. If not allowed, this
    // scenario will be considered as parsing failure.
    bool mAllowingShortenedFields = true;
    // Behavior when the number of extracted fields is greater than the number of Keys. Optional values include:
    // ● extend: Retain excess fields, and each excess field is added to the log as a separate field, with the field
    // name being __column$i__, where $i represents the sequence number of additional fields, starting from 0.
    // ● keep: Retain excess fields, but add surplus content as a whole field to the log, with the field name being
    // __column0__.
    // ● discard: Discard excess fields.
    OverflowedFieldsTreatment mOverflowedFieldsTreatment = OverflowedFieldsTreatment::EXTEND;
    bool mExtractingPartialFields = false;
    CommonParserOptions mCommonParserOptions;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override;

private:
    static const std::string s_mDiscardedFieldKey;

    bool ProcessEvent(const StringView& logPath, PipelineEventPtr& e);
    bool SplitString(const char* buffer,
                     int32_t begIdx,
                     int32_t endIdx,
                     std::vector<size_t>& colBegIdxs,
                     std::vector<size_t>& colLens);
    void AddLog(const StringView& key, const StringView& value, LogEvent& targetEvent, bool overwritten = true);

    char mSeparatorChar;
    bool mSourceKeyOverwritten = false;
    std::unique_ptr<DelimiterModeFsmParser> mDelimiterModeFsmParserPtr;

    int* mLogGroupSize = nullptr;
    int* mParseFailures = nullptr;
    CounterPtr mProcParseInSizeBytes;
    CounterPtr mProcParseOutSizeBytes;
    CounterPtr mProcDiscardRecordsTotal;
    CounterPtr mProcParseErrorTotal;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorParseDelimiterNativeUnittest;
#endif
};

} // namespace logtail
