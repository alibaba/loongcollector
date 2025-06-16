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

#include <thread>

#include "common/Flags.h"
#include "common/ProcParser.h"
#include "host_monitor/Constants.h"
#include "host_monitor/collector/ProcessEntityCollector.h"
#include "unittest/Unittest.h"

using namespace std;

DECLARE_FLAG_INT32(system_interface_default_cache_ttl);

namespace logtail {

class ProcessEntityCollectorUnittest : public testing::Test {
public:
    void TestGetSortProcessByCpu() const;
    void TestGetSortProcessByCpuFail() const;
    void TestGetProcessStat() const;
    void TestGetProcessStatFail() const;
    void TestGetProcessEntityID() const;

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

    void TearDown() override {
        bfs::remove_all("./1");
        bfs::remove_all("./stat");
        this_thread::sleep_for(std::chrono::milliseconds{500}); // wait system interface cache stale
    }
};

void ProcessEntityCollectorUnittest::TestGetSortProcessByCpu() const {
    PROCESS_DIR = "/proc";
    auto collector = ProcessEntityCollector();
    auto processes = vector<ExtendedProcessStatPtr>();
    collector.GetSortedProcess(processes, 3); // fist time will be ignored
    collector.GetSortedProcess(processes, 3);
    APSARA_TEST_EQUAL_FATAL(3, processes.size());
    auto prev = processes[0];
    for (auto i = 1UL; i < processes.size(); i++) {
        auto process = processes[i];
        APSARA_TEST_TRUE_FATAL(process->cpuInfo.percent >= prev->cpuInfo.percent);
        prev = process;
    }
}

void ProcessEntityCollectorUnittest::TestGetSortProcessByCpuFail() const {
    PROCESS_DIR = "/not_found_dir";
    std::this_thread::sleep_for(
        std::chrono::milliseconds{INT32_FLAG(system_interface_default_cache_ttl)}); // wait system interface cache stale
    auto collector = ProcessEntityCollector();
    auto processes = vector<ExtendedProcessStatPtr>();
    collector.GetSortedProcess(processes, 3); // fist time will be ignored
    collector.GetSortedProcess(processes, 3);
    APSARA_TEST_EQUAL_FATAL(0, processes.size());
}

void ProcessEntityCollectorUnittest::TestGetProcessStat() const {
    PROCESS_DIR = ".";
    auto collector = ProcessEntityCollector();
    auto pid = 1;
    bool isFirstCollect = true;
    auto processStat = collector.GetProcessStat(pid, isFirstCollect);
    APSARA_TEST_TRUE(processStat != nullptr);
    APSARA_TEST_EQUAL_FATAL(1, processStat->stat.pid);
    APSARA_TEST_EQUAL_FATAL(0, processStat->stat.parentPid);
    APSARA_TEST_EQUAL_FATAL("cat", processStat->stat.name);
    APSARA_TEST_TRUE_FATAL(isFirstCollect);

    ofstream ofs("./1/stat", std::ios::trunc);
    ofs << "1 (cat2) R 0 1 1 34816 1 4194560 1110 0 0 0 1 1 0 0 20 0 1 0 18938584 4505600 171 18446744073709551615 "
           "4194304 4238788 140727020025920 0 0 0 0 0 0 0 0 0 17 3 0 0 0 0 0 6336016 6337300 21442560 "
           "140727020027760 140727020027777 140727020027777 140727020027887 0";
    ofs.close();
    auto processStat2 = collector.GetProcessStat(pid, isFirstCollect);
    APSARA_TEST_TRUE_FATAL(processStat2 != nullptr);
    APSARA_TEST_EQUAL_FATAL(1, processStat2->stat.pid);
    APSARA_TEST_EQUAL_FATAL(0, processStat2->stat.parentPid);
    APSARA_TEST_EQUAL_FATAL("cat", processStat2->stat.name); // too quick collect, should reuse cache

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto processStat3 = collector.GetProcessStat(pid, isFirstCollect);
    APSARA_TEST_TRUE_FATAL(processStat3 != nullptr);
    APSARA_TEST_EQUAL_FATAL(1, processStat3->stat.pid);
    APSARA_TEST_EQUAL_FATAL(0, processStat3->stat.parentPid);
    APSARA_TEST_EQUAL_FATAL("cat2", processStat3->stat.name); // should read new stat
}

void ProcessEntityCollectorUnittest::TestGetProcessStatFail() const {
    PROCESS_DIR = "/not_found_dir";
    std::this_thread::sleep_for(
        std::chrono::milliseconds{INT32_FLAG(system_interface_default_cache_ttl)}); // wait system interface cache stale
    auto collector = ProcessEntityCollector();
    auto pid = 1;
    bool isFirstCollect = false;
    auto processStat = collector.GetProcessStat(pid, isFirstCollect);
    APSARA_TEST_TRUE(processStat == nullptr);
    APSARA_TEST_TRUE_FATAL(isFirstCollect);
}

void ProcessEntityCollectorUnittest::TestGetProcessEntityID() const {
    ProcessEntityCollector collect;
    APSARA_TEST_EQUAL(collect.GetProcessEntityID("123", "123", "123"), "f5bb0c8de146c67b44babbf4e6584cc0");
}

UNIT_TEST_CASE(ProcessEntityCollectorUnittest, TestGetSortProcessByCpu);
UNIT_TEST_CASE(ProcessEntityCollectorUnittest, TestGetSortProcessByCpuFail);
UNIT_TEST_CASE(ProcessEntityCollectorUnittest, TestGetProcessStat);
UNIT_TEST_CASE(ProcessEntityCollectorUnittest, TestGetProcessStatFail);
UNIT_TEST_CASE(ProcessEntityCollectorUnittest, TestGetProcessEntityID);

} // namespace logtail

UNIT_TEST_MAIN
