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

#include <json/json.h>

#include <algorithm>
#include <iostream>
#include <random>
#include <string>

#include "ebpf/type/table/AppTable.h"
#include "ebpf/type/table/BaseElements.h"
#include "ebpf/type/table/DataTable.h"
#include "ebpf/type/table/HttpTable.h"
#include "ebpf/type/table/NetTable.h"
#include "ebpf/type/table/ProcessTable.h"
#include "ebpf/type/table/StaticDataRow.h"
#include "logger/Logger.h"
#include "unittest/Unittest.h"

namespace logtail {
namespace ebpf {

class TableUnittest : public ::testing::Test {
public:
    void TestBasic();
    void TestDataElement();
    void TestDataTableSchema();
    void TestAppTable();
    void TestHttpTable();
    void TestBaseElements();
    void TestProcessTable();
    void TestNetTable();
    void TestCompileOperations();

protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ... 保留之前的测试方法 ...

void TableUnittest::TestProcessTable() {
    APSARA_TEST_TRUE(kProcessCacheTable.HasCol("exec_id"));
    APSARA_TEST_TRUE(kProcessCacheTable.HasCol("ktime"));
    APSARA_TEST_TRUE(kProcessCacheTable.HasCol("process_pid"));
    APSARA_TEST_TRUE(kProcessCacheTable.HasCol("uid"));
    APSARA_TEST_TRUE(kProcessCacheTable.HasCol("binary"));

    APSARA_TEST_EQUAL(kProcessCacheTableSize, std::size(kProcessCacheElements));

    APSARA_TEST_EQUAL(std::string(kProcessCacheTable.Name()), "process_cache_table");
}

void TableUnittest::TestNetTable() {
    // 测试 NetMetricsTable
    APSARA_TEST_TRUE(kNetMetricsTable.HasCol("host_name"));
    APSARA_TEST_TRUE(kNetMetricsTable.HasCol("app_id"));
    APSARA_TEST_TRUE(kNetMetricsTable.HasCol("ip"));
    APSARA_TEST_TRUE(kNetMetricsTable.HasCol("app"));
    APSARA_TEST_TRUE(kNetMetricsTable.HasCol("workload_kind"));
    APSARA_TEST_TRUE(kNetMetricsTable.HasCol("workload_name"));

    // 验证 NetMetricsTable 大小
    APSARA_TEST_EQUAL(kNetMetricsNum, std::size(kNetMetricsElements));

    // 验证 NetMetricsTable 表名和描述
    APSARA_TEST_EQUAL(std::string(kNetMetricsTable.Name()), "net_metrics");
    APSARA_TEST_EQUAL(std::string(kNetMetricsTable.Desc()), "net metrics table");
}

void TableUnittest::TestCompileOperations() {
    constexpr uint32_t appIdIdx = kConnTrackerTable.ColIndex(kAppId.Name());
    constexpr uint32_t appNameIdx = kConnTrackerTable.ColIndex(kAppName.Name());
    static_assert(appIdIdx == 2);
    static_assert(appNameIdx == 1);
    constexpr StringView s1 = "hello";
    constexpr StringView s2 = "hello";
    constexpr bool eq = s1 == s2;
    static_assert(eq, "static check pass ... ");

    StaticDataRow<&kConnTrackerTable> tb;
    tb.Set<kAppId>(StringView("hhh"));
    APSARA_TEST_EQUAL(tb.Get<kAppId>(), "hhh");
}

// 注册新增的测试用例
UNIT_TEST_CASE(TableUnittest, TestProcessTable);
UNIT_TEST_CASE(TableUnittest, TestNetTable);
UNIT_TEST_CASE(TableUnittest, TestCompileOperations);


} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
