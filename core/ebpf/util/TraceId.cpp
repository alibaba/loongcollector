// Copyright 2023 iLogtail Authors
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

#include "TraceId.h"

#include <array>
#include <random>
#include <memory>
#include <cstring>

namespace logtail {
namespace ebpf {

// std::array<uint8_t, 16> GenerateSpanID() {
//     std::random_device rd;
//     std::mt19937_64 generator(rd());
//     std::uniform_int_distribution<uint64_t> distribution(0, std::numeric_limits<uint64_t>::max());

//     auto result = std::array<uint8_t, 16>();
//     auto buf_size = result.size();

//     for (size_t i = 0; i < buf_size; i += sizeof(uint64_t)) {
//         uint64_t value = distribution(generator);

//         if (i + sizeof(uint64_t) <= buf_size) {
//             memcpy(&result[i], &value, sizeof(uint64_t));
//         } else {
//             memcpy(&result[i], &value, buf_size - i);
//         }
//     }
//     return result;
// }

}
}