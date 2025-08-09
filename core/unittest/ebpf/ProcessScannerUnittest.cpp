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

#include <cstdlib>

#include "ebpf/plugin/cpu_profiling/ProcessScanner.h"
#include "unittest/Unittest.h"

namespace logtail {
namespace ebpf {

class ProcessScannerUnittest : public testing::Test {
public:
    void TestStartAndStop();
    void TestWatchProcess();
    void TestSuspendAndResume();
};

void ProcessScannerUnittest::TestStartAndStop() {
    ProcessScanner::GetInstance()->Start();
    ProcessScanner::GetInstance()->Stop();
    ProcessScanner::GetInstance()->Start();
    ProcessScanner::GetInstance()->Stop();
}

void ProcessScannerUnittest::TestWatchProcess() {
    ProcessScanner::GetInstance()->Start();

    std::atomic_int pidsCount = 0;
    auto callback = [&](std::vector<uint32_t> pids_inner) {
        pidsCount += pids_inner.size();
    };

    // scan processes with "sleep" in cmdline
    pidsCount = 0;
    ProcessScanOption options{"test_watch", {"sleep.+"}, callback};
    ProcessScanner::GetInstance()->RegisterScan(options);
    std::system("sleep 0.5");
    APSARA_TEST_TRUE(pidsCount != 0);

    // remove the scan
    pidsCount = 0;
    ProcessScanner::GetInstance()->RemoveScan("test_watch");
    std::system("sleep 0.5");
    APSARA_TEST_TRUE(pidsCount == 0);

    // scan all processes
    pidsCount = 0;
    ProcessScanOption allOptions{"test_scan_all", {}, callback};
    ProcessScanner::GetInstance()->RegisterScan(allOptions);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    APSARA_TEST_TRUE(pidsCount != 0);

    ProcessScanner::GetInstance()->Stop();
}

void ProcessScannerUnittest::TestSuspendAndResume() {
    ProcessScanner::GetInstance()->Start();

    std::atomic_int pidsCount = 0;
    auto callback = [&](std::vector<uint32_t> pids_inner) {
        pidsCount += pids_inner.size();
    };

    // watch processes with "sleep" in cmdline
    pidsCount = 0;
    ProcessScanOption options{"test_watch_suspend", {"sleep.+"}, callback};
    ProcessScanner::GetInstance()->RegisterScan(options);
    std::system("sleep 0.5");
    APSARA_TEST_TRUE(pidsCount != 0);

    // now suspend the watcher
    pidsCount = 0;
    ProcessScanner::GetInstance()->Pause();
    std::system("sleep 0.5");
    // no new processes should be detected
    APSARA_TEST_TRUE(pidsCount == 0);

    // resume the watcher
    pidsCount = 0;
    ProcessScanner::GetInstance()->Resume();
    std::system("sleep 0.5");
    APSARA_TEST_TRUE(pidsCount != 0);

    ProcessScanner::GetInstance()->Stop();
}

UNIT_TEST_CASE(ProcessScannerUnittest, TestStartAndStop);
UNIT_TEST_CASE(ProcessScannerUnittest, TestWatchProcess);
UNIT_TEST_CASE(ProcessScannerUnittest, TestSuspendAndResume);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN