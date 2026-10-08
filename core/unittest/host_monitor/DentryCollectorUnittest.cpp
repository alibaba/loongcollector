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
#include "host_monitor/SystemInterface.h"
#include "host_monitor/collector/DentryCollector.h"
#include "models/MetricEvent.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class DentryCollectorUnittest : public testing::Test {
public:
    void TestCollectNormal() const;
    void TestCollectNullGroup() const;

protected:
    void SetUp() override {
        bfs::create_directories("./sys/fs");
        ofstream ofs("./sys/fs/dentry-state", std::ios::trunc);
        ofs << "293718\t218666\t45\t0\t287331\t0\n";
        ofs.close();
        PROCESS_DIR = ".";
    }

    void TearDown() override {
        bfs::remove_all("./sys");
        PROCESS_DIR = "/proc";
    }
};

void DentryCollectorUnittest::TestCollectNormal() const {
    auto dentryCollector = std::make_unique<DentryCollector>();
    HostMonitorContext collectContext("test",
                                      DentryCollector::sName,
                                      QueueKey{},
                                      0,
                                      std::chrono::seconds(15),
                                      CollectorInstance(std::move(dentryCollector)));
    collectContext.mCollectType = HostMonitorCollectType::kMultiValue;
    DentryCollector collector;
    APSARA_TEST_TRUE(collector.Init(collectContext));
    collectContext.mCountPerReport = 3;

    collectContext.SetTime(std::chrono::steady_clock::now(), time(nullptr));
    // First two calls with nullptr (intermediate collections in kMultiValue mode)
    APSARA_TEST_TRUE(collector.Collect(collectContext, nullptr));

    collectContext.SetTime(std::chrono::steady_clock::now(), time(nullptr));
    APSARA_TEST_TRUE(collector.Collect(collectContext, nullptr));

    // Third call with a valid group (ShouldGenerateMetric returns true)
    collectContext.SetTime(std::chrono::steady_clock::now(), time(nullptr));
    PipelineEventGroup group(make_shared<SourceBuffer>());
    APSARA_TEST_TRUE(collector.Collect(collectContext, &group));

    auto event = group.GetEvents()[0].Cast<MetricEvent>();
    auto maps = event.GetValue<UntypedMultiDoubleValues>()->mValues;

    // Verify all expected metric names exist
    vector<string> expected_names
        = {"dentry_nr", "dentry_unused", "dentry_age_limit", "dentry_want_pages", "dentry_negative"};
    for (const auto& name : expected_names) {
        APSARA_TEST_TRUE(maps.find(name) != maps.end());
    }

    // Verify metric values
    APSARA_TEST_EQUAL_FATAL(293718.0, maps["dentry_nr"].Value);
    APSARA_TEST_EQUAL_FATAL(218666.0, maps["dentry_unused"].Value);
    APSARA_TEST_EQUAL_FATAL(45.0, maps["dentry_age_limit"].Value);
    APSARA_TEST_EQUAL_FATAL(0.0, maps["dentry_want_pages"].Value);
    APSARA_TEST_EQUAL_FATAL(287331.0, maps["dentry_negative"].Value);

    // Verify tag
    APSARA_TEST_TRUE(event.HasTag("m"));
    APSARA_TEST_EQUAL(string("system.dentry"), event.GetTag("m"));
}

void DentryCollectorUnittest::TestCollectNullGroup() const {
    DentryCollector collector;
    HostMonitorContext collectContext("test",
                                      DentryCollector::sName,
                                      QueueKey{},
                                      0,
                                      std::chrono::seconds(15),
                                      CollectorInstance(std::make_unique<DentryCollector>()));
    collectContext.mCollectType = HostMonitorCollectType::kMultiValue;
    APSARA_TEST_TRUE(collector.Init(collectContext));
    collectContext.mCountPerReport = 3;

    collectContext.SetTime(std::chrono::steady_clock::now(), time(nullptr));
    // Passing nullptr should return true (intermediate collection, no output
    // needed)
    APSARA_TEST_TRUE(collector.Collect(collectContext, nullptr));
}

UNIT_TEST_CASE(DentryCollectorUnittest, TestCollectNormal);
UNIT_TEST_CASE(DentryCollectorUnittest, TestCollectNullGroup);

} // namespace logtail

UNIT_TEST_MAIN
