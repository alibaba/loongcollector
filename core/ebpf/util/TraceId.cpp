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

#include <cstring>

#include <array>
#include <iomanip>
#include <memory>
#include <random>
#include <sstream>

#include "spdlog/spdlog.h"

namespace logtail {
namespace ebpf {

std::string BytesToHexString(const uint8_t* bytes, size_t length) {
    std::string result;
    result.reserve(length * 2);
    for (size_t i = 0; i < length; ++i) {
        fmt::format_to(std::back_inserter(result), "{:02x}", bytes[i]);
    }
    return result;
}

char randHexChar() {
    thread_local static std::random_device rd;
    thread_local static std::mt19937 generator(rd());
    thread_local static std::uniform_int_distribution<int> distribution(0, 15);
    static const char hexChars[] = "0123456789abcdef";
    return hexChars[distribution(generator)];
}

std::string RandHexStr(int len) {
    std::string res(len, '0'); // Initialize string with '0' of length 'len'
    for (int i = 0; i < len; ++i) { // Corrected loop condition
        res[i] = randHexChar();
    }
    return res;
}

std::array<uint8_t, 32> GenerateTraceID() {
    std::random_device rd;
    std::mt19937_64 generator(rd());
    std::uniform_int_distribution<uint64_t> distribution(0, std::numeric_limits<uint64_t>::max());

    auto result = std::array<uint8_t, 32>();
    auto buf_size = result.size();

    for (size_t i = 0; i < buf_size; i += sizeof(uint64_t)) {
        uint64_t value = distribution(generator);

        if (i + sizeof(uint64_t) <= buf_size) {
            memcpy(&result[i], &value, sizeof(uint64_t));
        } else {
            memcpy(&result[i], &value, buf_size - i);
        }
    }
    return result;
}

std::string FromSpanId(const std::array<uint8_t, 16>& spanId) {
    return BytesToHexString(spanId.data(), spanId.size());
}

std::string FromTraceId(const std::array<uint8_t, 32>& traceId) {
    return BytesToHexString(traceId.data(), traceId.size());
}

std::array<uint8_t, 16> GenerateSpanID() {
    std::random_device rd;
    std::mt19937_64 generator(rd());
    std::uniform_int_distribution<uint64_t> distribution(0, std::numeric_limits<uint64_t>::max());

    auto result = std::array<uint8_t, 16>();
    auto buf_size = result.size();

    for (size_t i = 0; i < buf_size; i += sizeof(uint64_t)) {
        uint64_t value = distribution(generator);

        if (i + sizeof(uint64_t) <= buf_size) {
            memcpy(&result[i], &value, sizeof(uint64_t));
        } else {
            memcpy(&result[i], &value, buf_size - i);
        }
    }
    return result;
}

} // namespace ebpf
} // namespace logtail
