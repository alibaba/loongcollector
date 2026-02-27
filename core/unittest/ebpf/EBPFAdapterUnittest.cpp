// Copyright 2025 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ebpf/EBPFAdapter.h"
#include "unittest/Unittest.h"

using namespace logtail::ebpf;

class EBPFAdapterUnittest : public testing::Test {
public:
    void TestLogPrinter();
};

void EBPFAdapterUnittest::TestLogPrinter() {
    EBPFAdapter adapter;
    adapter.Init();

    // 使用 lambda 模拟可变参数调用
    auto callWithFormat = [](eBPFLogType level, EBPFAdapter& adapter, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        adapter.mLogPrinter(int16_t(level), fmt, args);
        va_end(args);
    };

    callWithFormat(eBPFLogType::NAMI_LOG_TYPE_WARN, adapter, "%s", "test warning log message");
    callWithFormat(eBPFLogType::NAMI_LOG_TYPE_INFO, adapter, "%s", "test info log message");
    callWithFormat(eBPFLogType::NAMI_LOG_TYPE_DEBUG, adapter, "%s", "test debug log message");
    callWithFormat((eBPFLogType)3, adapter, "%s", "test error log message");
}

UNIT_TEST_CASE(EBPFAdapterUnittest, TestLogPrinter)

UNIT_TEST_MAIN
