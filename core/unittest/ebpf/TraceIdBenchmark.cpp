/*
 * Copyright 2025 iLogtail Authors
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
#include <spdlog/fmt/fmt.h>

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "ebpf/util/TraceId.h"
#include "unittest/Unittest.h"

using namespace std;
using namespace logtail;
using namespace logtail::ebpf;

template <size_t N>
std::string FromRandom64IDUsingArrayFmt(const std::array<uint64_t, N>& id) {
    constexpr size_t charsPerInt = 16; // 每个 uint64_t 需要 16 个字符
    std::array<char, N * charsPerInt + 1> buffer{}; // +1 for null terminator

    for (size_t i = 0; i < N; i++) {
        fmt::format_to(buffer.begin(), "{:016x}", id[i]);
    }

    buffer[N * charsPerInt] = '\0'; // Null terminator
    return std::string(buffer.data());
}

template <size_t N>
std::string FromRandom64IDUsingStringFmt(const std::array<uint64_t, N>& id) {
    std::string result;
    result.reserve(N << 4);
    for (size_t i = 0; i < N; i++) {
        fmt::format_to(std::back_inserter(result), "{:016x}", id[i]);
    }
    return result;
}

template <size_t N>
std::string FromRandom64IDUsingStringStream(const std::array<uint64_t, N>& id) {
    std::stringstream ss;
    for (size_t i = 0; i < N; i++) {
        ss << std::setfill('0') << std::setw(16) << std::hex << id[i];
    }
    return ss.str();
}

class TraceIdBenchmark : public testing::Test {
public:
    void TestUsingArrayHex();
    void TestUsingArrayFmt();
    void TestUsingStringFmt();
    void TestUsingStringStream();

protected:
    void SetUp() override {
        for (int i = 0; i < 1000000; i++) {
            mTraceIDs.push_back(GenerateTraceID());
        }
    }

private:
    std::vector<std::array<uint64_t, 4>> mTraceIDs;
};

void TraceIdBenchmark::TestUsingArrayHex() {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; i++) {
        TraceIDToString(mTraceIDs[i]);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "[TestUsingArray] elapsed: " << elapsed.count() << " seconds" << std::endl;
}

void TraceIdBenchmark::TestUsingArrayFmt() {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; i++) {
        FromRandom64IDUsingArrayFmt(mTraceIDs[i]);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "[TestUsingFmt] elapsed: " << elapsed.count() << " seconds" << std::endl;
}


void TraceIdBenchmark::TestUsingStringFmt() {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; i++) {
        FromRandom64IDUsingStringFmt(mTraceIDs[i]);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "[TestUsingFmt] elapsed: " << elapsed.count() << " seconds" << std::endl;
}

void TraceIdBenchmark::TestUsingStringStream() {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; i++) {
        FromRandom64IDUsingStringStream(mTraceIDs[i]);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "[TestUsingStringStream] elapsed: " << elapsed.count() << " seconds" << std::endl;
}

UNIT_TEST_CASE(TraceIdBenchmark, TestUsingArrayHex)
UNIT_TEST_CASE(TraceIdBenchmark, TestUsingArrayFmt)
UNIT_TEST_CASE(TraceIdBenchmark, TestUsingStringFmt)
UNIT_TEST_CASE(TraceIdBenchmark, TestUsingStringStream)

UNIT_TEST_MAIN
