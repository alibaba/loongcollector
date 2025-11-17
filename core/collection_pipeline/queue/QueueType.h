// Copyright 2024 iLogtail Authors
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

namespace logtail {

// Queue type enum, used to specify the type of process queue
enum class QueueType {
    COUNT_BOUNDED, // Count-based bounded queue
    BYTES_BOUNDED, // Byte-based bounded queue
    CIRCULAR, // Circular queue
};

} // namespace logtail

