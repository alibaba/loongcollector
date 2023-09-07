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
#include "models/PipelineEvent.h"

namespace logtail {

class MetricEvent : public PipelineEvent {
public:
    static std::unique_ptr<MetricEvent> CreateEvent(std::shared_ptr<SourceBuffer>& sb);

    // for debug and test
    Json::Value ToJson() const override;
    bool FromJson(const Json::Value&) override;
    uint64_t EventsSizeBytes() override;
private:
    MetricEvent();
};

} // namespace logtail