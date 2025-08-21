/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <string>

#include "collection_pipeline/plugin/interface/Processor.h"
#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"
#include "models/RawEvent.h"
#include "models/SpanEvent.h"

namespace logtail {
class ProcessorParseFromPBNative : public Processor {
public:
    static const std::string sName;
    static const std::vector<std::string> sSupportedProtocols;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value&) override;
    void Process(PipelineEventGroup&) override;
    void Process(std::vector<PipelineEventGroup>&) override;

protected:
    bool IsSupportedEvent(const PipelineEventPtr&) const override;

private:
    std::string mProtocol;
    CounterPtr mOutFailedEventGroupsTotal;
    CounterPtr mOutSuccessfulEventGroupsTotal;
    CounterPtr mDiscardedEventsTotal;
    CounterPtr mOutSuccessfulEventsTotal;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorParseFromPBNativeUnittest;
#endif
};

} // namespace logtail
