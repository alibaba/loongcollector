// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "SpanConstants.h"

namespace logtail {

    const std::string DEFAULT_TRACE_TAG_TRACE_ID = "traceId";
    const std::string DEFAULT_TRACE_TAG_SPAN_ID = "spanId";
    const std::string DEFAULT_TRACE_TAG_PARENT_ID = "parentSpanId";
    const std::string DEFAULT_TRACE_TAG_SPAN_NAME = "spanName";
    const std::string DEFAULT_TRACE_TAG_SERVICE_NAME = "serviceName";
    const std::string DEFAULT_TRACE_TAG_START_TIME_NANO = "startTime";
    const std::string DEFAULT_TRACE_TAG_END_TIME_NANO = "endTime";
    const std::string DEFAULT_TRACE_TAG_DURATION = "duration";
    const std::string DEFAULT_TRACE_TAG_ATTRIBUTES = "attributes";
    const std::string DEFAULT_TRACE_TAG_RESOURCE = "resources";
    const std::string DEFAULT_TRACE_TAG_LINKS = "links";
    const std::string DEFAULT_TRACE_TAG_EVENTS = "events";
    const std::string DEFAULT_TRACE_TAG_TIMESTAMP = "timestamp";
    const std::string DEFAULT_TRACE_TAG_STATUS_CODE = "statusCode";
    const std::string DEFAULT_TRACE_TAG_STATUS_MESSAGE = "statusMessage";
    const std::string DEFAULT_TRACE_TAG_SPAN_KIND = "kind";
    const std::string DEFAULT_TRACE_TAG_TRACE_STATE = "traceState";
    const std::string DEFAULT_TRACE_TAG_SPAN_EVENT_NAME = "name";
#ifdef __ENTERPRISE__
    // for arms
    const std::string DEFAULT_TRACE_TAG_APP_ID = "pid";
    const std::string DEFAULT_TRACE_TAG_IP = "ip";
#endif

} // namespace logtail