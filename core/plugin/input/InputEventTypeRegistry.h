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

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "models/PipelineEvent.h"

namespace logtail {

enum class InputEventMask : uint8_t {
    NONE = 0,
    LOG = 1 << 0,
    METRIC = 1 << 1,
    SPAN = 1 << 2,
};

inline InputEventMask operator|(InputEventMask lhs, InputEventMask rhs) {
    return static_cast<InputEventMask>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

inline bool HasMask(InputEventMask value, InputEventMask expected) {
    return (static_cast<uint8_t>(value) & static_cast<uint8_t>(expected)) == static_cast<uint8_t>(expected);
}

struct InputEventTypeEntry {
    std::string_view mInputType;
    InputEventMask mMask;
    std::string_view mProducerPath;
    std::string_view mNote;
};

// Native input contract matrix used by tests and code review.
// This table documents which PipelineEvent types each C++ input can emit:
// - LOG: LogEvent
// - METRIC: MetricEvent
// - SPAN: SpanEvent
const std::vector<InputEventTypeEntry>& GetNativeInputEventTypeEntries();

bool InputProducesEventType(std::string_view inputType, PipelineEvent::Type eventType);
std::string InputEventMaskToString(InputEventMask mask);

} // namespace logtail
