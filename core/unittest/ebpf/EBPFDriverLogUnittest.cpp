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

#include "ebpf/driver/Log.h"
#include "unittest/Unittest.h"

using namespace logtail::ebpf;

class EBPFDriverLogUnittest : public testing::Test {
public:
    void TestLogPrinter();
};

void EBPFDriverLogUnittest::TestLogPrinter() {
    // 使用 lambda 模拟可变参数调用
    auto callWithFormat = [](enum libbpf_print_level level, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        libbpf_printf(level, fmt, args);
        va_end(args);
    };

    callWithFormat(LIBBPF_WARN, "%s", "test warning log message");
    callWithFormat(LIBBPF_INFO, "%s", "test info log message");
    callWithFormat(LIBBPF_DEBUG, "%s", "test debug log message");
    callWithFormat((enum libbpf_print_level)3, "%s", "test error log message");
}

UNIT_TEST_CASE(EBPFDriverLogUnittest, TestLogPrinter)

UNIT_TEST_MAIN
