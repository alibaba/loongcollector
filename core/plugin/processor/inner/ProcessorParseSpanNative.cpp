/*
 * Copyright 2024 iLogtail Authors
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

#include "plugin/processor/inner/ProcessorParseSpanNative.h"
#include "Logger.h"
#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"
#include "models/RawEvent.h"
#include "protobuf/models/ProtocolConversion.h"
#include "protobuf/models/pipeline_event_group.pb.h"

using namespace std;

namespace logtail {

const string ProcessorParseSpanNative::sName = "processor_parse_span_native";

// only for inner processor
bool ProcessorParseSpanNative::Init(const Json::Value&) {
    return true;
}

void ProcessorParseSpanNative::Process(PipelineEventGroup& eventGroup) {
    // TODO support for multi schema version
    const auto& e = eventGroup.GetEvents().at(0);
    if (!IsSupportedEvent(e)) {
        LOG_ERROR(sLogger, ("unsupported event type", "pipelineEventGroup[0] is not a RawEvent"));
        return;
    }
    const auto& sourceEvent = e.Cast<RawEvent>();

    std::string errMsg;
    models::PipelineEventGroup peg;
    if (peg.ParseFromString(sourceEvent.GetContent().data())) {
        eventGroup.MutableEvents().clear();
        TransferPBToPipelineEventGroup(peg, eventGroup, errMsg);
    }

    if (!errMsg.empty()) {
        LOG_ERROR(sLogger, ("error transfer PB to PipelineEventGroup", errMsg));
        eventGroup.MutableEvents().clear();
        return;
    }
}

bool ProcessorParseSpanNative::IsSupportedEvent(const PipelineEventPtr& event) const {
    return event.Is<RawEvent>();
}

} // namespace logtail
