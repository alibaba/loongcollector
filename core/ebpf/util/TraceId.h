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

#pragma once

#include <array>
#include <memory>

#include "spdlog/spdlog.h"

namespace logtail::ebpf {

template <size_t N>
std::string FromRandom64ID(const std::array<uint64_t, N>& id) {
    std::string result;
    result.reserve(N << 4);
    for (size_t i = 0; i < N; i++) {
        fmt::format_to(std::back_inserter(result), "{:016x}", id[i]);
    }
    return result;
}

std::array<uint64_t, 4> GenerateTraceID();
std::array<uint64_t, 2> GenerateSpanID();

} // namespace logtail::ebpf
