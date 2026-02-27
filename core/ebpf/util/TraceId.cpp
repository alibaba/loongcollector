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

template <size_t N>
std::string FromRandom64ID(const std::array<uint64_t, N>& id) {
    constexpr size_t charsPerInt = 16; // 每个 uint64_t 需要 16 个字符
    std::array<char, N * charsPerInt + 1> buffer{}; // +1 for null terminator

    for (size_t i = 0; i < N; ++i) {
        constexpr const char* hexChars = "0123456789abcdef";
        uint64_t value = id[i];
        for (int j = charsPerInt - 1; j >= 0; --j) {
            buffer[i * charsPerInt + j] = hexChars[value & 0xF];
            value >>= 4;
        }
    }

    buffer[N * charsPerInt] = '\0'; // Null terminator
    return std::string(buffer.data());
}

std::array<uint64_t, 4> GenerateTraceID() {
    std::array<uint64_t, 4> result{};
    GenerateRand64(result);
    return result;
}

std::array<uint64_t, 2> GenerateSpanID() {
    std::array<uint64_t, 2> result{};
    GenerateRand64(result);
    return result;
}

std::string TraceIDToString(const std::array<uint64_t, 4>& traceID) {
    return FromRandom64ID(traceID);
}

std::string SpanIDToString(const std::array<uint64_t, 2>& spanID) {
    return FromRandom64ID(spanID);
}

} // namespace ebpf
} // namespace logtail
