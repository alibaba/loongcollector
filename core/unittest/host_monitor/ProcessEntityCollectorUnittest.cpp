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

#include "host_monitor/Constants.h"
#include "host_monitor/collector/ProcessEntityCollector.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class ProcessEntityCollectorUnittest : public testing::Test {
public:
    void TestGetNewProcessStat() const;
    void TestSortProcessByCpu() const;
    void TestGetProcessEntityID() const;
};

void ProcessEntityCollectorUnittest::TestGetNewProcessStat() const {
    PROCESS_DIR = ".";
    auto collector = ProcessEntityCollector();
    auto ptr = collector.GetNewProcessStat(1);
    APSARA_TEST_NOT_EQUAL(nullptr, ptr);
    APSARA_TEST_EQUAL(1, ptr->pid);
    APSARA_TEST_EQUAL("cat", ptr->name);
}

void ProcessEntityCollectorUnittest::TestSortProcessByCpu() const {
    PROCESS_DIR = "/proc";
    auto collector = ProcessEntityCollector();
    auto processes = vector<ProcessStatPtr>();
    collector.GetSortedProcess(processes, 5); // fist time will be ignored
    collector.GetSortedProcess(processes, 5);
    APSARA_TEST_EQUAL(5, processes.size());
    auto prev = processes[0];
    for (auto i = 1; i < processes.size(); i++) {
        auto process = processes[i];
        APSARA_TEST_TRUE(process->cpuInfo.percent <= prev->cpuInfo.percent);
        prev = process;
    }
}

void ProcessEntityCollectorUnittest::TestGetProcessEntityID() const {
    ProcessEntityCollector collect;
    collect.mHostEntityID = "123";
    APSARA_TEST_EQUAL(collect.GetProcessEntityID("123", "123"), "f5bb0c8de146c67b44babbf4e6584cc0");
}

UNIT_TEST_CASE(ProcessEntityCollectorUnittest, TestGetNewProcessStat);
UNIT_TEST_CASE(ProcessEntityCollectorUnittest, TestSortProcessByCpu);
UNIT_TEST_CASE(ProcessEntityCollectorUnittest, TestGetProcessEntityID);

} // namespace logtail

UNIT_TEST_MAIN
