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

#include "common/TimeUtil.h"
#include "models/LogEvent.h"
#include "pipeline/plugin/interface/Processor.h"
#include "plugin/processor/CommonParserOptions.h"

namespace logtail {

class ProcessorParseApsaraNative : public Processor {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config) override;
    void Process(PipelineEventGroup& logGroup) override;

    // Source field name.
    std::string mSourceKey;
    // The time zone to which the log time belongs. The format is GMT+HH:MM (Eastern Zone) or GMT-HH:MM (Western Zone).
    std::string mTimezone;
    CommonParserOptions mCommonParserOptions;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override;

private:
    bool
    ProcessEvent(const StringView& logPath, PipelineEventPtr& e, LogtailTime& lastLogTime, StringView& timeStrCache);
    void AddLog(const StringView& key, const StringView& value, LogEvent& targetEvent, bool overwritten = true);
    time_t
    ApsaraEasyReadLogTimeParser(StringView& buffer, StringView& timeStr, LogtailTime& lastLogTime, int64_t& microTime);
    bool IsPrefixString(const std::string& all, const StringView& prefix);
    int32_t ParseApsaraBaseFields(const StringView& buffer, LogEvent& sourceEvent);

    int32_t mLogTimeZoneOffsetSecond = 0;

    CounterPtr mDiscardedEventsTotal;
    CounterPtr mOutFailedEventsTotal;
    CounterPtr mOutKeyNotFoundEventsTotal;
    CounterPtr mOutSuccessfulEventsTotal;
    CounterPtr mHistoryFailureTotal;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorParseApsaraNativeUnittest;
#endif
};

} // namespace logtail
