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
#include <locale>

#include "json/json.h"
#include "json/reader.h"

#include "Logger.h"
#include "SpanEvent.h"
#include "common/StringTools.h"
#include "models/MetricEvent.h"
#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"
#include "models/RawEvent.h"
#include "protobuf/models/ProtocolConversion.h"
#include "protobuf/models/pipeline_event_group.pb.h"
#include "google/protobuf/message.h"
#include "protobuf/models/span_event.pb.h"

using namespace std;
namespace logtail {

const string ProcessorParseSpanNative::sName = "processor_parse_span_native";

// only for inner processor
bool ProcessorParseSpanNative::Init(const Json::Value&) {
    return true;
}

void ProcessorParseSpanNative::Process(PipelineEventGroup& eventGroup) {
    // TODO support for multi schema version
    EventsContainer& events = eventGroup.MutableEvents();
    EventsContainer newEvents;
    newEvents.reserve(events.size());

    for (auto& e : events) {
        ProcessEvent(e, newEvents, eventGroup);
    }
    events.swap(newEvents);
}

bool ProcessorParseSpanNative::IsSupportedEvent(const PipelineEventPtr& event) const {
    return event.Is<RawEvent>();
}

bool ProcessorParseSpanNative::ProcessEvent(PipelineEventPtr& e,
                                            EventsContainer& newEvents,
                                            PipelineEventGroup& eventGroup) {
    if (!IsSupportedEvent(e)) {
        return false;
    }
    auto& sourceEvent = e.Cast<RawEvent>();

    std::string errMsg;
    std::unique_ptr<SpanEvent> spanEvent = eventGroup.CreateSpanEvent(true);
    models::SpanEvent se;
    if (se.ParseFromString(sourceEvent.GetContent().data())) {
        TransferPBToSpanEvent(se, *spanEvent, errMsg);
        newEvents.emplace_back(std::move(spanEvent), true, nullptr);
    }
    return true;
}

} // namespace logtail
