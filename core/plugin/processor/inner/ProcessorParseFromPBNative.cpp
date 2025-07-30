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

#include "plugin/processor/inner/ProcessorParseFromPBNative.h"

#include "common/ParamExtractor.h"
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

const std::vector<std::string> ProcessorParseFromPBNative::sSupportedProtocols = {
    "LoongSuite",
};

// only for inner processor
bool ProcessorParseFromPBNative::Init(const Json::Value& config) {
    mOutFailedEventGroupsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_FAILED_EVENT_GROUPS_TOTAL);
    mOutSuccessfulEventGroupsTotal
        = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_SUCCESSFUL_EVENT_GROUPS_TOTAL);
    mDiscardedEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_DISCARDED_EVENTS_TOTAL);
    mOutSuccessfulEventsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_OUT_SUCCESSFUL_EVENTS_TOTAL);

    std::string errMsg;
    if (!GetMandatoryStringParam(config, "Protocol", mProtocol, errMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    auto it = std::find(sSupportedProtocols.begin(), sSupportedProtocols.end(), mProtocol);
    if (it == sSupportedProtocols.end()) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           "Unsupported protocol '" + mProtocol,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    return true;
}

void ProcessorParseFromPBNative::Process(PipelineEventGroup& eventGroup) {
    if (eventGroup.GetEvents().empty()) {
        LOG_WARNING(sLogger, ("unsupported event group", "pipelineEventGroup is empty"));
        return;
    }

    auto originalRawEvents = std::move(eventGroup.MutableEvents());
    eventGroup.MutableEvents().clear();

    for (const auto& e : originalRawEvents) {
        if (!IsSupportedEvent(e)) {
            LOG_WARNING(sLogger, ("unsupported event type", "pipelineEventGroup is not a RawEvent, will be discarded"));
            ADD_COUNTER(mDiscardedEventsTotal, 1);
            continue;
        }
        const auto& sourceEvent = e.Cast<RawEvent>();

        std::string errMsg;
        models::PipelineEventGroup pbGroup;
        const auto& content = sourceEvent.GetContent();
        if (!pbGroup.ParseFromArray(content.data(), content.size())
            || !TransferPBToPipelineEventGroup(pbGroup, eventGroup, errMsg)) {
            LOG_WARNING(sLogger, ("error transfer PB to PipelineEventGroup", errMsg)("content size", content.size()));
            ADD_COUNTER(mOutFailedEventGroupsTotal, 1);
            continue;
        }
        ADD_COUNTER(mOutSuccessfulEventGroupsTotal, 1);
    }
    ADD_COUNTER(mOutSuccessfulEventsTotal, eventGroup.GetEvents().size());
}

bool ProcessorParseFromPBNative::IsSupportedEvent(const PipelineEventPtr& event) const {
    return event.Is<RawEvent>();
}

} // namespace logtail
