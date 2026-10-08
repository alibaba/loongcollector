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

#include <filesystem>

#include "host_monitor/Constants.h"
#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/HostMonitorTypes.h"
#include "host_monitor/collector/FsStatCollector.h"
#include "models/MetricEvent.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class FsStatCollectorUnittest : public testing::Test {
public:
    void TestCollectNullGroup() const;
    void TestCollectWithRealMounts() const;

protected:
    void SetUp() override {
        // Create a fake mtab with a known /dev/ entry pointing to /
        // ETC_DIR / ETC_MTAB = "./mtab" when ETC_DIR = "."
        ofstream ofs("./mtab", std::ios::trunc);
        ofs << "/dev/root / ext4 rw,relatime 0 0\n";
        ofs << "proc /proc proc rw,nosuid,nodev,noexec,relatime 0 0\n";
        ofs << "sysfs /sys sysfs rw,nosuid,nodev,noexec,relatime 0 0\n";
        ofs.close();
        mOrigEtcDir = ETC_DIR.string();
        ETC_DIR = ".";
    }

    void TearDown() override {
        bfs::remove("./mtab");
        ETC_DIR = mOrigEtcDir;
    }

private:
    std::string mOrigEtcDir;
};

void FsStatCollectorUnittest::TestCollectNullGroup() const {
    auto fsStatCollector = std::make_unique<FsStatCollector>();
    HostMonitorContext collectContext("test",
                                      FsStatCollector::sName,
                                      QueueKey{},
                                      0,
                                      std::chrono::seconds(15),
                                      CollectorInstance(std::move(fsStatCollector)));
    collectContext.mCollectType = HostMonitorCollectType::kMultiValue;
    FsStatCollector collector;
    APSARA_TEST_TRUE(collector.Init(collectContext));
    collectContext.mCountPerReport = 3;

    collectContext.SetTime(std::chrono::steady_clock::now(), time(nullptr));
    // nullptr group should return true in kMultiValue mode
    APSARA_TEST_TRUE(collector.Collect(collectContext, nullptr));
}

void FsStatCollectorUnittest::TestCollectWithRealMounts() const {
    auto fsStatCollector = std::make_unique<FsStatCollector>();
    HostMonitorContext collectContext("test",
                                      FsStatCollector::sName,
                                      QueueKey{},
                                      0,
                                      std::chrono::seconds(15),
                                      CollectorInstance(std::move(fsStatCollector)));
    collectContext.mCollectType = HostMonitorCollectType::kMultiValue;
    FsStatCollector collector;
    APSARA_TEST_TRUE(collector.Init(collectContext));
    collectContext.mCountPerReport = 3;

    // Intermediate calls with nullptr
    collectContext.SetTime(std::chrono::steady_clock::now(), time(nullptr));
    APSARA_TEST_TRUE(collector.Collect(collectContext, nullptr));
    collectContext.SetTime(std::chrono::steady_clock::now(), time(nullptr));
    APSARA_TEST_TRUE(collector.Collect(collectContext, nullptr));

    // Final call with a valid group
    collectContext.SetTime(std::chrono::steady_clock::now(), time(nullptr));
    PipelineEventGroup group(make_shared<SourceBuffer>());
    APSARA_TEST_TRUE(collector.Collect(collectContext, &group));

    // Should have at least 1 event (the /dev/root mount)
    APSARA_TEST_TRUE(group.GetEvents().size() >= 1);

    auto event = group.GetEvents()[0].Cast<MetricEvent>();
    auto maps = event.GetValue<UntypedMultiDoubleValues>()->mValues;

    // Verify all expected metric names exist
    vector<string> expected_names = {"f_bsize", "f_blocks", "f_bfree", "f_bavail", "f_files", "f_ffree"};
    for (const auto& name : expected_names) {
        APSARA_TEST_TRUE(maps.find(name) != maps.end());
    }

    // Verify metric values are non-zero (root filesystem should have data)
    APSARA_TEST_TRUE(maps["f_bsize"].Value > 0);
    APSARA_TEST_TRUE(maps["f_blocks"].Value > 0);
    APSARA_TEST_TRUE(maps["f_files"].Value > 0);

    // Verify tags
    APSARA_TEST_TRUE(event.HasTag("m"));
    APSARA_TEST_EQUAL(string("system.fsstat"), event.GetTag("m"));
    APSARA_TEST_TRUE(event.HasTag("device"));
}

UNIT_TEST_CASE(FsStatCollectorUnittest, TestCollectNullGroup);
UNIT_TEST_CASE(FsStatCollectorUnittest, TestCollectWithRealMounts);

} // namespace logtail

UNIT_TEST_MAIN
