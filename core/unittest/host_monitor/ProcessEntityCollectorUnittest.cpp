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
    void TestGetSystemBootSeconds() const;

protected:
    void SetUp() override {
        bfs::create_directories("./1");
        ofstream ofs("./1/stat", std::ios::trunc);
        ofs << "1 (cat) R 0 1 1 34816 1 4194560 1110 0 0 0 1 1 0 0 20 0 1 0 18938584 4505600 171 18446744073709551615 "
               "4194304 4238788 140727020025920 0 0 0 0 0 0 0 0 0 17 3 0 0 0 0 0 6336016 6337300 21442560 "
               "140727020027760 140727020027777 140727020027777 140727020027887 0";
        ofs.close();
        ofstream ofs2("./stat", std::ios::trunc);
        ofs2 << "btime 1731142542";
        ofs2.close();
    }
};

void ProcessEntityCollectorUnittest::TestGetNewProcessStat() const {
    PROCESS_DIR = ".";
    auto collector = ProcessEntityCollector();
    auto ptr = collector.ReadNewProcessStat(1);
    APSARA_TEST_NOT_EQUAL(nullptr, ptr);
    APSARA_TEST_EQUAL(1, ptr->pid);
    APSARA_TEST_EQUAL("cat", ptr->name);
}

void ProcessEntityCollectorUnittest::TestSortProcessByCpu() const {
    PROCESS_DIR = "/proc";
    auto collector = ProcessEntityCollector();
    auto processes = vector<ProcessStatPtr>();
    collector.GetSortedProcess(processes, 3); // fist time will be ignored
    collector.GetSortedProcess(processes, 3);
    APSARA_TEST_EQUAL(3, processes.size());
    auto prev = processes[0];
    for (auto i = 1; i < processes.size(); i++) {
        auto process = processes[i];
        APSARA_TEST_TRUE(process->cpuInfo.percent >= prev->cpuInfo.percent);
        prev = process;
    }
}

void ProcessEntityCollectorUnittest::TestGetProcessEntityID() const {
    ProcessEntityCollector collect;
    APSARA_TEST_EQUAL(collect.GetProcessEntityID("123", "123", "123"), "f5bb0c8de146c67b44babbf4e6584cc0");
}

void ProcessEntityCollectorUnittest::TestGetSystemBootSeconds() const {
    PROCESS_DIR = ".";
    ProcessEntityCollector collect;
    APSARA_TEST_EQUAL(1731142542, collect.GetHostSystemBootTime());
}

UNIT_TEST_CASE(ProcessEntityCollectorUnittest, TestGetNewProcessStat);
UNIT_TEST_CASE(ProcessEntityCollectorUnittest, TestSortProcessByCpu);
UNIT_TEST_CASE(ProcessEntityCollectorUnittest, TestGetProcessEntityID);
UNIT_TEST_CASE(ProcessEntityCollectorUnittest, TestGetSystemBootSeconds);

} // namespace logtail

UNIT_TEST_MAIN
