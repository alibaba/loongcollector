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

#include <string>
#include <vector>

#include "file_server/WatchManager.h"
#include "file_server/event_listener/EventListener.h"
#include "monitor/MetricManager.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

// Records every underlying watch call so the test can assert WatchManager
// forwards one-to-one to EventListener without altering counts or return
// values (pass-through equivalence).
class CountingWatchManager : public WatchManager {
public:
    CountingWatchManager() : WatchManager() {}

    int mAddCount = 0;
    int mRemoveCount = 0;
    std::vector<std::string> mAddedPaths;
    std::vector<int> mRemovedWds;
    int mNextWd = 100;

protected:
    int DoAddWatch(const char* path) override {
        ++mAddCount;
        mAddedPaths.emplace_back(path);
        return mNextWd++;
    }
    bool DoRemoveWatch(int wd) override {
        ++mRemoveCount;
        mRemovedWds.push_back(wd);
        return true;
    }
};

class WatchManagerUnittest : public ::testing::Test {
public:
    // AcquireWatch/ReleaseWatch must forward to the underlying watch syscalls
    // exactly once per call and return their result unchanged.
    void TestAcquireReleasePassThrough() {
        CountingWatchManager wm;

        int wd0 = wm.AcquireWatch("/tmp/watch_manager_ut/a");
        APSARA_TEST_EQUAL_FATAL(1, wm.mAddCount);
        APSARA_TEST_EQUAL_FATAL(string("/tmp/watch_manager_ut/a"), wm.mAddedPaths[0]);
        APSARA_TEST_EQUAL_FATAL(100, wd0); // returns underlying wd verbatim

        int wd1 = wm.AcquireWatch("/tmp/watch_manager_ut/b");
        APSARA_TEST_EQUAL_FATAL(2, wm.mAddCount);
        APSARA_TEST_EQUAL_FATAL(101, wd1);

        bool released = wm.ReleaseWatch(wd0);
        APSARA_TEST_TRUE_FATAL(released);
        APSARA_TEST_EQUAL_FATAL(1, wm.mRemoveCount);
        APSARA_TEST_EQUAL_FATAL(wd0, wm.mRemovedWds[0]);
    }

    // The skeleton adds no dedup: adding the same path twice forwards twice,
    // preserving the pre-refactor behavior where EventDispatcher owns dedup.
    void TestNoDedupAtWatchManager() {
        CountingWatchManager wm;

        wm.AcquireWatch("/tmp/watch_manager_ut/same");
        wm.AcquireWatch("/tmp/watch_manager_ut/same");
        APSARA_TEST_EQUAL_FATAL(2, wm.mAddCount);
        APSARA_TEST_EQUAL_FATAL(wm.mAddedPaths[0], wm.mAddedPaths[1]);
    }

    // WatchManager must forward watches to the very same EventListener that
    // EventDispatcher reads and removes events through; otherwise a watch
    // acquired via WatchManager would be invisible to the dispatcher's event
    // loop. Both sides obtain the listener from EventListener::GetInstance() --
    // a Meyers singleton (function-local static, private ctor) that yields one
    // process-wide instance. Lock that invariant here so a future refactor
    // giving WatchManager its own listener fails loudly instead of silently
    // desyncing the two.
    void TestSharedEventListenerInstance() {
        WatchManager* wm = WatchManager::GetInstance();
        // GetInstance() is stable across calls (same singleton every time)...
        APSARA_TEST_EQUAL_FATAL(EventListener::GetInstance(), EventListener::GetInstance());
        // ...and the runner stores exactly that canonical instance, which is the
        // same one EventDispatcher's ctor captures via EventListener::GetInstance().
        APSARA_TEST_EQUAL_FATAL(EventListener::GetInstance(), wm->mEventListener);
    }

    // Constructing WatchManager registers a runner_name = "watch_manager"
    // MetricsRecordRef and commits it to WriteMetrics.
    void TestMetricsRecordRegistered() {
        WatchManager* wm = WatchManager::GetInstance();
        APSARA_TEST_TRUE_FATAL(wm->GetMetricsRecordRef().HasLabel(
            METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_WATCH_MANAGER));

        // GetHead() is private to WriteMetrics; iterate committed records via the
        // public DoSnapshot() instead. It returns caller-owned copies of the
        // undeleted records (deleted ones are already filtered out), so we free
        // the returned list afterwards to avoid leaking.
        bool found = false;
        MetricsRecord* snapshot = WriteMetrics::GetInstance()->DoSnapshot();
        for (MetricsRecord* record = snapshot; record != nullptr; record = record->GetNext()) {
            for (auto label = record->GetLabels()->begin(); label != record->GetLabels()->end(); ++label) {
                if (label->first == METRIC_LABEL_KEY_RUNNER_NAME
                    && label->second == METRIC_LABEL_VALUE_RUNNER_NAME_WATCH_MANAGER) {
                    found = true;
                    break;
                }
            }
            if (found) {
                break;
            }
        }
        while (snapshot != nullptr) {
            MetricsRecord* next = snapshot->GetNext();
            delete snapshot;
            snapshot = next;
        }
        APSARA_TEST_TRUE_FATAL(found);
    }
};

APSARA_UNIT_TEST_CASE(WatchManagerUnittest, TestAcquireReleasePassThrough, 0);
APSARA_UNIT_TEST_CASE(WatchManagerUnittest, TestNoDedupAtWatchManager, 1);
APSARA_UNIT_TEST_CASE(WatchManagerUnittest, TestSharedEventListenerInstance, 2);
APSARA_UNIT_TEST_CASE(WatchManagerUnittest, TestMetricsRecordRegistered, 3);

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
