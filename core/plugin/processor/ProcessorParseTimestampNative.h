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
#include "pipeline/plugin/interface/Processor.h"

namespace logtail {
class ProcessorParseTimestampNative : public Processor {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config) override;
    void Process(PipelineEventGroup& logGroup) override;

    // Source field name.
    std::string mSourceKey;
    // Log time format. %Y/%m/%d %H:%M:%S
    std::string mSourceFormat;
    // The time zone to which the log time belongs. The format is GMT+HH:MM (Eastern Zone) or GMT-HH:MM (Western Zone).
    std::string mSourceTimezone;
    int32_t mSourceYear = -1;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override;

private:
    /// @return false if data need to be discarded
    bool ProcessEvent(StringView logPath, PipelineEventPtr& e, LogtailTime& logTime, StringView& timeStrCache);
    /// @return false if parse time failed
    bool ParseLogTime(const StringView& curTimeStr, // str to parse
                      const StringView& logPath,
                      LogtailTime& logTime,
                      uint64_t& preciseTimestamp,
                      StringView& timeStr // cache
    );
    bool IsPrefixString(const StringView& all, const StringView& prefix);

    int32_t mLogTimeZoneOffsetSecond = 0;

    int* mParseTimeFailures = nullptr;
    int* mHistoryFailures = nullptr;

    CounterPtr mDiscardedEventsTotal;
    CounterPtr mOutFailedEventsTotal;
    CounterPtr mOutKeyNotFoundEventsTotal;
    CounterPtr mOutSuccessfulEventsTotal;
    CounterPtr mHistoryFailureTotal;
    
#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorParseTimestampNativeUnittest;
    friend class ProcessorParseLogTimeUnittest;
#endif
};

} // namespace logtail
