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
#include <string>

namespace logtail {
    extern const std::string DEFAULT_TRACE_TAG_TRACE_ID;
    extern const std::string DEFAULT_TRACE_TAG_SPAN_ID;
    extern const std::string DEFAULT_TRACE_TAG_PARENT_ID;
    extern const std::string DEFAULT_TRACE_TAG_SPAN_NAME;
    extern const std::string DEFAULT_TRACE_TAG_SERVICE_NAME;
    extern const std::string DEFAULT_TRACE_TAG_START_TIME_NANO;
    extern const std::string DEFAULT_TRACE_TAG_END_TIME_NANO;
    extern const std::string DEFAULT_TRACE_TAG_DURATION;
    extern const std::string DEFAULT_TRACE_TAG_ATTRIBUTES;
    extern const std::string DEFAULT_TRACE_TAG_RESOURCE;
    extern const std::string DEFAULT_TRACE_TAG_LINKS;
    extern const std::string DEFAULT_TRACE_TAG_EVENTS;
    extern const std::string DEFAULT_TRACE_TAG_TIMESTAMP;
    extern const std::string DEFAULT_TRACE_TAG_STATUS_CODE;
    extern const std::string DEFAULT_TRACE_TAG_STATUS_MESSAGE;
    extern const std::string DEFAULT_TRACE_TAG_SPAN_KIND;
    extern const std::string DEFAULT_TRACE_TAG_TRACE_STATE;
    extern const std::string DEFAULT_TRACE_TAG_SPAN_EVENT_NAME ;
#ifdef __ENTERPRISE__
    // for arms
    extern const std::string DEFAULT_TRACE_TAG_APP_ID;
    extern const std::string DEFAULT_TRACE_TAG_IP;
#endif

} // namespace logtail