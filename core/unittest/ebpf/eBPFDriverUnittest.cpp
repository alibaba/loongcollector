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

#include "ebpf/plugin/network_observer/Worker.h"
#include "unittest/Unittest.h"

namespace logtail {
namespace ebpf {

class eBPFDriverUnittest : public ::testing::Test {
public:
    // void TestWorkerPool();
    // void TestNetDataHandler();

protected:
    void SetUp() override {}
    void TearDown() override {}
};

// UNIT_TEST_CASE(eBPFDriverUnittest, TestWorkerPool);
// UNIT_TEST_CASE(eBPFDriverUnittest, TestNetDataHandler);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
