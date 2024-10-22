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

#include "models/PipelineEventGroup.h"
#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/SpanEvent.h"
#include "protobuf/models/pipeline_event_group.pb.h"
#include "protobuf/models/log_event.pb.h"
#include "protobuf/models/metric_event.pb.h"
#include "protobuf/models/span_event.pb.h"

namespace logtail {

bool TransferPBToPipelineEventGroup(const models::PipelineEventGroup& src, PipelineEventGroup& dst, std::string& errMsg);
bool TransferPBToLogEvent(const models::LogEvent& src, LogEvent& dst, std::string& errMsg);
bool TransferPBToMetricEvent(const models::MetricEvent& src, MetricEvent& dst, std::string& errMsg);
bool TransferPBToSpanEvent(const models::SpanEvent& src, SpanEvent& dst, std::string& errMsg);

bool TransferPipelineEventGroupToPB(const PipelineEventGroup& src, models::PipelineEventGroup& dst, std::string& errMsg);
bool TransferLogEventToPB(const LogEvent& src, models::LogEvent& dst, std::string& errMsg);
bool TransferMetricEventToPB(const MetricEvent& src, models::MetricEvent& dst, std::string& errMsg);
bool TransferSpanEventToPB(const SpanEvent& src, models::SpanEvent& dst, std::string& errMsg);

} // namespace logtail
