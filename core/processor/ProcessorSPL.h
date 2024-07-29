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

#include <string>

#include "monitor/LogtailMetric.h"
#include "plugin/interface/Processor.h"

namespace apsara::sls::spl {
class SplPipeline;
}

namespace logtail {

class ProcessorSPL : public Processor {
public:
    static const std::string sName;
    const std::string& Name() const override { return sName; }

    // Source field name.
    std::string mSpl;
    uint32_t mTimeoutMills = 1000;
    uint32_t mMaxMemoryBytes = 50*1024*1024;

    bool Init(const Json::Value& config) override;
    void Process(std::vector<PipelineEventGroup>& logGroupList) override;
    void Process(PipelineEventGroup& logGroup) override;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override;

private:
    std::shared_ptr<apsara::sls::spl::SplPipeline> mSPLPipelinePtr;

    CounterPtr mSplExcuteErrorCount;
    CounterPtr mSplExcuteTimeoutErrorCount;
    CounterPtr mSplExcuteMemoryExceedErrorCount;

    CounterPtr mProcessMicros;
    CounterPtr mInputMicros;
    CounterPtr mOutputMicros;
    IntGaugePtr mMemPeakBytes;
    CounterPtr mTotalTaskCount;
    CounterPtr mSuccTaskCount;
    CounterPtr mFailTaskCount;
};
} // namespace logtail
