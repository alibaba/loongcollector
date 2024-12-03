/*
 * Copyright 2024 iLogtail Authors
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

#include "PipelineEventPtr.h"
#include "Processor.h"

namespace logtail {
class ProcessorHostMetaNative : public Processor {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config) override;
    void Process(PipelineEventGroup& group) override;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& event) const override;

private:
    void ProcessEvent(PipelineEventGroup& group, PipelineEventPtr&& e, EventsContainer& newEvents);

    const std::string GetProcessEntityID(StringView pid, StringView createTime);

    std::string mDomain;
    std::string mEntityType;
    std::string mHostEntityID;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorHostMetaNativeUnittest;
#endif
};

} // namespace logtail
