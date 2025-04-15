// Copyright 2025 iLogtail Authors
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

#include <cstring>

#include <iomanip>
#include <random>

namespace logtail {
namespace ebpf {

template <size_t N>
void GenerateRand64(std::array<uint64_t, N>& result) {
    thread_local static std::random_device sRd;
    thread_local static std::mt19937_64 sGenerator(sRd());
    thread_local static std::uniform_int_distribution<uint64_t> sDistribution(0, std::numeric_limits<uint64_t>::max());

    for (size_t i = 0; i < N; i++) {
        result[i] = sDistribution(sGenerator);
    }
}

std::array<uint64_t, 4> GenerateTraceID() {
    std::array<uint64_t, 4> result{};
    GenerateRand64<4>(result);
    return result;
}

std::array<uint64_t, 2> GenerateSpanID() {
    std::array<uint64_t, 2> result{};
    GenerateRand64<2>(result);
    return result;
}

} // namespace ebpf
} // namespace logtail
