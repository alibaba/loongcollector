// Copyright 2026 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstdint>

#include "agentsight.h"

// LoongCollector can be built with an older AgentSight SDK while loading a
// newer libagentsight.so at runtime. Remove this fallback after the pinned SDK
// header defines AGENTSIGHT_HAS_READ_V2.
#ifndef AGENTSIGHT_HAS_READ_V2
enum AgentsightEventType : uint32_t {
    AGENTSIGHT_EVENT_TYPE_HTTPS = 1,
    AGENTSIGHT_EVENT_TYPE_LLM = 2,
    AGENTSIGHT_EVENT_TYPE_SECURITY = 3,
};

struct AgentsightEvent {
    AgentsightEventType event_type;
    uint16_t schema_version;
    uint64_t timestamp_ns;
    const char* payload_json;
    uint32_t payload_json_len;
};

using agentsight_event_callback_fn = void (*)(const AgentsightEvent* data, void* user_data);
#endif
