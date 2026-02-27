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

#include <chrono>

#include "common/Flags.h"
#include "common/ParamExtractor.h"
#include "logger/Logger.h"
#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"
#include "models/RawEvent.h"
#include "monitor/metric_models/MetricTypes.h"
#include "protobuf/models/ManualPBParser.h"
#include "protobuf/models/ProtocolConversion.h"
#include "protobuf/models/pipeline_event_group.pb.h"

DEFINE_FLAG_BOOL(debug_sls_serializer, "", false);

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

void ProcessorParseFromPBNative::Process(std::vector<PipelineEventGroup>& eventGroupList) {
    std::vector<PipelineEventGroup> originalEventGroupList = std::move(eventGroupList);
    eventGroupList.clear();

    for (auto& rawEventGroup : originalEventGroupList) {
        if (rawEventGroup.GetEvents().empty()) {
            continue;
        }

        for (const auto& e : rawEventGroup.GetEvents()) {
            if (!IsSupportedEvent(e)) {
                LOG_WARNING(sLogger,
                            ("unsupported event type", "pipelineEventGroup is not a RawEvent, will be discarded"));
                ADD_COUNTER(mDiscardedEventsTotal, 1);
                continue;
            }
            const auto& sourceEvent = e.Cast<RawEvent>();

            std::string errMsg;
            auto eventGroup = PipelineEventGroup(std::make_shared<SourceBuffer>());

            // parse event group from raw event
            const auto& content = sourceEvent.GetContent();

            ManualPBParser parser(reinterpret_cast<const uint8_t*>(content.data()), content.size(), false);
            if (!parser.ParsePipelineEventGroup(eventGroup, errMsg)) {
                LOG_WARNING(
                    sLogger,
                    ("error parsing PipelineEventGroup with manual parser", errMsg)("content size", content.size()));

                if (BOOL_FLAG(debug_sls_serializer)) {
                    models::PipelineEventGroup pbGroup;
                    if (!pbGroup.ParseFromArray(content.data(), content.size())
                        || !TransferPBToPipelineEventGroup(pbGroup, eventGroup, errMsg)) {
                        LOG_WARNING(sLogger,
                                    ("error transfer PB to PipelineEventGroup with native parser",
                                     errMsg)("content size", content.size()));
                        ADD_COUNTER(mOutFailedEventGroupsTotal, 1);
                        continue;
                    } else {
                        LOG_ERROR(
                            sLogger,
                            ("failed to parse pipeline event group with manual parser, but success with native parser",
                             std::string(content.data(), content.size())));
                    }
                } else {
                    ADD_COUNTER(mOutFailedEventGroupsTotal, 1);
                    continue;
                }
            }

            // inherit metadata from original event group
            // SetAllMetadata is not a deep copy, so we need to iterate and set the metadata to the new event group
            for (const auto& [key, value] : rawEventGroup.GetAllMetadata()) {
                eventGroup.SetMetadata(key, value);
            }
            ADD_COUNTER(mOutSuccessfulEventsTotal, eventGroup.GetEvents().size());

            // append event group to eventGroupList
            eventGroupList.emplace_back(std::move(eventGroup));
            ADD_COUNTER(mOutSuccessfulEventGroupsTotal, 1);
        }
    }
}

void ProcessorParseFromPBNative::Process(PipelineEventGroup& eventGroup) {
    LOG_ERROR(sLogger,
              ("ProcessorParseFromPBNative error",
               "unexpected enter in ProcessorParseFromPBNative::Process(PipelineEventGroup& eventGroup)")(
                  "project", mContext->GetProjectName())("logstore", mContext->GetLogstoreName())(
                  "region", mContext->GetRegion())("configName", mContext->GetConfigName()));
}

bool ProcessorParseFromPBNative::IsSupportedEvent(const PipelineEventPtr& event) const {
    return event.Is<RawEvent>();
}

} // namespace logtail
