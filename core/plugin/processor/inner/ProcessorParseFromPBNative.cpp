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

#include "plugin/processor/inner/ProcessorParseFromPBNative.h"

#include "logger/Logger.h"
#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"
#include "models/RawEvent.h"
#include "monitor/metric_models/MetricTypes.h"
#include "protobuf/models/ProtocolConversion.h"
#include "protobuf/models/pipeline_event_group.pb.h"

using namespace std;

namespace logtail {

const string ProcessorParseFromPBNative::sName = "processor_parse_from_pb_native";

// only for inner processor
bool ProcessorParseFromPBNative::Init(const Json::Value&) {
    mOutFailedEventGroupsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_FAILED_EVENT_GROUPS_TOTAL);
    mOutSuccessfulEventGroupsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_SUCCESSFUL_EVENT_GROUPS_TOTAL);
    return true;
}

void ProcessorParseFromPBNative::Process(PipelineEventGroup& eventGroup) {
    if (eventGroup.GetEvents().empty()) {
        LOG_WARNING(sLogger, ("unsupported event type", "pipelineEventGroup is empty"));
        return;
    }
    const auto& e = eventGroup.GetEvents().at(0);
    if (!IsSupportedEvent(e)) {
        LOG_WARNING(sLogger, ("unsupported event type", "pipelineEventGroup[0] is not a RawEvent"));
        return;
    }
    const auto& sourceEvent = e.Cast<RawEvent>();

    std::string errMsg;
    models::PipelineEventGroup pbGroup;
    
    if (pbGroup.ParseFromString(sourceEvent.GetContent().data())) {
        eventGroup.MutableEvents().clear();
        TransferPBToPipelineEventGroup(pbGroup, eventGroup, errMsg);
    } else {
        eventGroup.MutableEvents().clear();
        ADD_COUNTER(mOutFailedEventGroupsTotal, 1);
        return;
    }

    if (!errMsg.empty()) {
        LOG_WARNING(sLogger, ("error transfer PB to PipelineEventGroup", errMsg));
        ADD_COUNTER(mOutFailedEventGroupsTotal, 1);
        eventGroup.MutableEvents().clear();
        return;
    }
    ADD_COUNTER(mOutSuccessfulEventGroupsTotal, 1)
}

bool ProcessorParseFromPBNative::IsSupportedEvent(const PipelineEventPtr& event) const {
    return event.Is<RawEvent>();
}

} // namespace logtail
