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

#include "processor/ProcessorParseJsonNative.h"
#include "common/Constants.h"
#include "models/LogEvent.h"
#include "plugin/ProcessorInstance.h"


namespace logtail {

bool ProcessorParseJsonNative::Init(const ComponentConfig& componentConfig) {
    SetMetricsRecordRef(Name(), componentConfig.GetId());
    return true;
}

void ProcessorParseJsonNative::Process(PipelineEventGroup& logGroup) {
    return;
}

bool ProcessorParseJsonNative::IsSupportedEvent(const PipelineEventPtr& e) {
    return e.Is<LogEvent>();
}

} // namespace logtail