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
#include "ebpf/type/table/FileSecurityTable.h"
#include "ebpf/type/table/HttpTable.h"
#include "ebpf/type/table/NetTable.h"
#include "ebpf/type/table/NetworkSecurityTable.h"
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
    void TestFileSecurityTable();
    void TestNetworkSecurityTable();
    void TestNetTable();
    void TestCompileOperations();

protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ... 保留之前的测试方法 ...

void TableUnittest::TestProcessTable() {
    // 测试 ProcessCacheTable
    APSARA_TEST_TRUE(kProcessCacheTable.HasCol("exec_id"));
    APSARA_TEST_TRUE(kProcessCacheTable.HasCol("ktime"));
    APSARA_TEST_TRUE(kProcessCacheTable.HasCol("process_pid"));
    APSARA_TEST_TRUE(kProcessCacheTable.HasCol("uid"));
    APSARA_TEST_TRUE(kProcessCacheTable.HasCol("binary"));

    // 验证表大小
    APSARA_TEST_EQUAL(kProcessCacheTableSize, std::size(kProcessCacheElements));

    // 测试 ProcessSecurityTable
    APSARA_TEST_TRUE(kProcessSecurityTable.HasCol("event_time"));
    APSARA_TEST_TRUE(kProcessSecurityTable.HasCol("call_name"));
    APSARA_TEST_TRUE(kProcessSecurityTable.HasCol("event_type"));

    // 验证表大小
    APSARA_TEST_EQUAL(kProcessSecurityTableSize, std::size(kProcessSecurityElements));

    // 验证表名
    APSARA_TEST_EQUAL(std::string(kProcessCacheTable.Name()), "process_cache_table");
    APSARA_TEST_EQUAL(std::string(kProcessSecurityTable.Name()), "process_security_table");
}

void TableUnittest::TestFileSecurityTable() {
    // 测试 FileSecurityTable 的列存在性
    APSARA_TEST_TRUE(kFileSecurityTable.HasCol("exec_id"));
    APSARA_TEST_TRUE(kFileSecurityTable.HasCol("process_pid"));
    APSARA_TEST_TRUE(kFileSecurityTable.HasCol("uid"));
    APSARA_TEST_TRUE(kFileSecurityTable.HasCol("binary"));
    APSARA_TEST_TRUE(kFileSecurityTable.HasCol("container_id"));
    APSARA_TEST_TRUE(kFileSecurityTable.HasCol("path"));

    // 验证表大小
    APSARA_TEST_EQUAL(kFileSecurityTableSize, std::size(kFileSecurityElements));

    // 验证表名
    APSARA_TEST_EQUAL(std::string(kFileSecurityTable.Name()), "file_security_table");

    // 验证特定字段的属性
    auto pathIdx = kFileSecurityTable.ColIndex("path");
    APSARA_TEST_TRUE(pathIdx < kFileSecurityTableSize);
    APSARA_TEST_EQUAL(std::string(kFileSecurityElements[pathIdx].Desc()), "file path");
}

void TableUnittest::TestNetworkSecurityTable() {
    // 测试 NetworkSecurityTable 的列存在性
    APSARA_TEST_TRUE(kNetworkSecurityTable.HasCol("exec_id"));
    APSARA_TEST_TRUE(kNetworkSecurityTable.HasCol("process_pid"));
    APSARA_TEST_TRUE(kNetworkSecurityTable.HasCol("container_id"));
    APSARA_TEST_TRUE(kNetworkSecurityTable.HasCol("source.addr"));
    APSARA_TEST_TRUE(kNetworkSecurityTable.HasCol("dest.addr"));
    APSARA_TEST_TRUE(kNetworkSecurityTable.HasCol("source.port"));
    APSARA_TEST_TRUE(kNetworkSecurityTable.HasCol("dest.port"));
    APSARA_TEST_TRUE(kNetworkSecurityTable.HasCol("state"));
    APSARA_TEST_TRUE(kNetworkSecurityTable.HasCol("protocol"));

    // 验证表大小
    APSARA_TEST_EQUAL(kNetworkSecurityTableSize, std::size(kNetworkSecurityElements));

    // 验证表名
    APSARA_TEST_EQUAL(std::string(kNetworkSecurityTable.Name()), "network_security_table");

    // 验证网络特定字段的属性
    auto saddrIdx = kNetworkSecurityTable.ColIndex("source.addr");
    APSARA_TEST_TRUE(saddrIdx < kNetworkSecurityTableSize);
    APSARA_TEST_EQUAL(std::string(kNetworkSecurityElements[saddrIdx].Desc()), "source address");
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

    // 测试 NetTable
    APSARA_TEST_TRUE(kNetTable.HasCol("ip"));
    APSARA_TEST_TRUE(kNetTable.HasCol("app_id"));
    APSARA_TEST_TRUE(kNetTable.HasCol("local_addr"));
    APSARA_TEST_TRUE(kNetTable.HasCol("remote_addr"));
    APSARA_TEST_TRUE(kNetTable.HasCol("remote_port"));

    // 验证 NetTable 表名和描述
    APSARA_TEST_EQUAL(std::string(kNetTable.Name()), "net_record");
    APSARA_TEST_EQUAL(std::string(kNetTable.Desc()), "net events");

    // 验证字段属性
    auto remoteAddrIdx = kNetTable.ColIndex("remote_addr");
    APSARA_TEST_TRUE(remoteAddrIdx < std::size(kNetElements));
    APSARA_TEST_EQUAL(std::string(kNetElements[remoteAddrIdx].Desc()), "IP address of the remote endpoint.");
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

    constexpr std::array<size_t, 2> elements
        = {kConnTrackerTable.ColIndex(kAppId.Name()), kConnTrackerTable.ColIndex(kAppName.Name())};
    for (size_t i = 0; i < elements.size(); i++) {
        // constexpr uint32_t idx = kConnTrackerTable.ColIndex(elements[i].Name());
        // constexpr uint32_t sIdx = 2;
        // tb.GetTagValue<sIdx>();
    }
}

// 注册新增的测试用例
UNIT_TEST_CASE(TableUnittest, TestProcessTable);
UNIT_TEST_CASE(TableUnittest, TestFileSecurityTable);
UNIT_TEST_CASE(TableUnittest, TestNetworkSecurityTable);
UNIT_TEST_CASE(TableUnittest, TestNetTable);
UNIT_TEST_CASE(TableUnittest, TestCompileOperations);


} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
