// Copyright 2024 iLogtail Authors
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

#include "ProcessCollector.h"
#include "host_monitor/Constants.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class ProcessCollectorUnittest : public testing::Test {
public:
    void TestReadProcessStat() const;
    void TestSortProcessByCpu() const;
};

void ProcessCollectorUnittest::TestReadProcessStat() const {
    PROCESS_DIR = ".";
    auto collector = ProcessCollector();
    auto ptr = collector.ReadProcessStat(1);
    APSARA_TEST_NOT_EQUAL(nullptr, ptr);
    APSARA_TEST_EQUAL(1, ptr->pid);
    APSARA_TEST_EQUAL("cat", ptr->name);
}

void ProcessCollectorUnittest::TestSortProcessByCpu() const {
    PROCESS_DIR = "/proc";
    auto collector = ProcessCollector();
    auto processes = vector<ProcessStatPtr>();
    collector.SortProcessByCpu(processes, 5); // fist time will be ignored
    collector.SortProcessByCpu(processes, 5);
    APSARA_TEST_EQUAL(5, processes.size());
    auto prev = processes[0];
    for (auto i = 1; i < processes.size(); i++) {
        auto process = processes[i];
        APSARA_TEST_TRUE(process->cpuInfo.percent <= prev->cpuInfo.percent);
        prev = process;
    }
}

UNIT_TEST_CASE(ProcessCollectorUnittest, TestReadProcessStat);
UNIT_TEST_CASE(ProcessCollectorUnittest, TestSortProcessByCpu);

} // namespace logtail

UNIT_TEST_MAIN
