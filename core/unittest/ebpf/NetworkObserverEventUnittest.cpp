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

#include "ebpf/type/NetworkObserverEvent.h"
#include "unittest/Unittest.h"

namespace logtail {
namespace ebpf {

class NetworkObserverEventUnittest : public ::testing::Test {
public:
    void TestConnId();
    void TestConnIdHash();
    void TestCaseInsensitiveLess();
    void TestHeadersMap();
    void TestConnStatsRecord();
    void TestHttpRecord();
    void TestAppMetricData();
    void TestNetMetricData();
    void TestConnIdMove();
    void TestConnIdFromConnectId();
    void TestHeadersMapCaseInsensitive();
    void TestHttpRecordTimestamps();
    void TestHttpRecordStatus();
    void TestAbstractNetRecord();

protected:
    void SetUp() override {}
    void TearDown() override {}
};

void NetworkObserverEventUnittest::TestConnId() {
    // 测试构造函数
    ConnId id1(1, 1000, 123456);
    APSARA_TEST_EQUAL(id1.fd, 1);
    APSARA_TEST_EQUAL(id1.tgid, 1000);
    APSARA_TEST_EQUAL(id1.start, 123456);

    // 测试拷贝构造
    ConnId id2(id1);
    APSARA_TEST_EQUAL(id2.fd, id1.fd);
    APSARA_TEST_EQUAL(id2.tgid, id1.tgid);
    APSARA_TEST_EQUAL(id2.start, id1.start);

    // 测试移动构造
    ConnId id3(std::move(ConnId(2, 2000, 234567)));
    APSARA_TEST_EQUAL(id3.fd, 2);
    APSARA_TEST_EQUAL(id3.tgid, 2000);
    APSARA_TEST_EQUAL(id3.start, 234567);

    // 测试相等运算符
    APSARA_TEST_TRUE(id1 == id2);
    APSARA_TEST_FALSE(id1 == id3);
}

void NetworkObserverEventUnittest::TestConnIdHash() {
    ConnId id1(1, 1000, 123456);
    ConnId id2(1, 1000, 123456);
    ConnId id3(2, 2000, 234567);

    ConnIdHash hasher;
    // 相同的 ConnId 应该有相同的哈希值
    APSARA_TEST_EQUAL(hasher(id1), hasher(id2));
    // 不同的 ConnId 应该有不同的哈希值
    APSARA_TEST_TRUE(hasher(id1) != hasher(id3));
}

void NetworkObserverEventUnittest::TestCaseInsensitiveLess() {
    CaseInsensitiveLess comparator;

    // 测试相同字符串的不同大小写
    APSARA_TEST_FALSE(comparator(std::string("hello"), std::string("HELLO")));
    APSARA_TEST_FALSE(comparator(std::string("HELLO"), std::string("hello")));

    // 测试不同字符串
    APSARA_TEST_TRUE(comparator(std::string("hello"), std::string("world")));
    APSARA_TEST_TRUE(comparator(std::string("HELLO"), std::string("WORLD")));
}

void NetworkObserverEventUnittest::TestHeadersMap() {
    HeadersMap headers;

    // 测试大小写不敏感的键
    headers.insert({"Content-Type", "application/json"});
    headers.insert({"CONTENT-TYPE", "text/plain"});

    // 验证插入的值
    auto range = headers.equal_range("content-type");
    std::vector<std::string> values;
    for (auto it = range.first; it != range.second; ++it) {
        values.push_back(it->second);
    }

    APSARA_TEST_EQUAL(values.size(), 2);
    APSARA_TEST_TRUE(std::find(values.begin(), values.end(), "application/json") != values.end());
    APSARA_TEST_TRUE(std::find(values.begin(), values.end(), "text/plain") != values.end());
}

void NetworkObserverEventUnittest::TestConnStatsRecord() {
    ConnId id(1, 1000, 123456);
    ConnStatsRecord record(id);

    // 测试基本属性
    APSARA_TEST_EQUAL(record.GetEventType(), EventType::CONN_STATS_EVENT);
    APSARA_TEST_EQUAL(record.GetAggregateType(), AggregateType::NETWORK);
    APSARA_TEST_FALSE(record.IsError());
    APSARA_TEST_FALSE(record.IsSlow());
    APSARA_TEST_EQUAL(record.GetStatusCode(), 0);

    // 测试表模式
    APSARA_TEST_EQUAL(record.GetMetricsTableSchema().name(), "net_metrics");
    APSARA_TEST_EQUAL(record.GetTableSchema().name(), "net_record");
}

void NetworkObserverEventUnittest::TestHttpRecord() {
    ConnId id(1, 1000, 123456);
    HttpRecord record(std::move(id));

    // 测试基本属性
    record.SetPath("/api/v1/test");
    record.SetMethod("GET");
    record.SetStatusCode("200");
    record.SetProtocolVersion("HTTP/1.1");

    APSARA_TEST_EQUAL(record.GetEventType(), EventType::HTTP_EVENT);
    APSARA_TEST_EQUAL(record.GetPath(), "/api/v1/test");
    APSARA_TEST_EQUAL(record.GetMethod(), "GET");
    APSARA_TEST_EQUAL(record.GetStatusCode(), 200);
    APSARA_TEST_EQUAL(record.GetProtocolVersion(), "HTTP/1.1");
    APSARA_TEST_FALSE(record.IsError());

    // 测试错误状态码
    record.SetStatusCode("404");
    APSARA_TEST_TRUE(record.IsError());
    APSARA_TEST_EQUAL(record.GetStatusCode(), 404);

    // 测试请求头
    HeadersMap reqHeaders;
    reqHeaders.insert({"Content-Type", "application/json"});
    record.SetReqHeaderMap(reqHeaders);
    APSARA_TEST_EQUAL(record.GetReqHeaderMap().size(), 1);

    // 测试响应头
    HeadersMap respHeaders;
    respHeaders.insert({"Content-Length", "100"});
    record.SetRespHeaderMap(respHeaders);
    APSARA_TEST_EQUAL(record.GetRespHeaderMap().size(), 1);
}

void NetworkObserverEventUnittest::TestAppMetricData() {
    ConnId id(1, 1000, 123456);
    AppMetricData data(id, "test_span");

    // 测试基本属性设置和获取
    data.mCount = 100;
    data.mSum = 1000.0;
    data.mSlowCount = 5;
    data.mErrCount = 2;
    data.m2xxCount = 80;
    data.m3xxCount = 10;
    data.m4xxCount = 8;
    data.m5xxCount = 2;

    APSARA_TEST_EQUAL(data.mCount, 100);
    APSARA_TEST_EQUAL(data.mSum, 1000.0);
    APSARA_TEST_EQUAL(data.mSlowCount, 5);
    APSARA_TEST_EQUAL(data.mErrCount, 2);
    APSARA_TEST_EQUAL(data.m2xxCount, 80);
    APSARA_TEST_EQUAL(data.m3xxCount, 10);
    APSARA_TEST_EQUAL(data.m4xxCount, 8);
    APSARA_TEST_EQUAL(data.m5xxCount, 2);
}

void NetworkObserverEventUnittest::TestNetMetricData() {
    ConnId id(1, 1000, 123456);
    NetMetricData data(id);

    // 测试基本属性设置和获取
    data.mDropCount = 10;
    data.mRetransCount = 5;
    data.mRtt = 100;
    data.mRecvBytes = 1024;
    data.mSendBytes = 2048;
    data.mRecvPkts = 100;
    data.mSendPkts = 200;

    APSARA_TEST_EQUAL(data.mDropCount, 10);
    APSARA_TEST_EQUAL(data.mRetransCount, 5);
    APSARA_TEST_EQUAL(data.mRtt, 100);
    APSARA_TEST_EQUAL(data.mRecvBytes, 1024);
    APSARA_TEST_EQUAL(data.mSendBytes, 2048);
    APSARA_TEST_EQUAL(data.mRecvPkts, 100);
    APSARA_TEST_EQUAL(data.mSendPkts, 200);
}

void NetworkObserverEventUnittest::TestConnIdMove() {
    ConnId id1(1, 1000, 123456);
    ConnId id2(std::move(id1));
    APSARA_TEST_EQUAL(id2.fd, 1);
    APSARA_TEST_EQUAL(id2.tgid, 1000);
    APSARA_TEST_EQUAL(id2.start, 123456);
}

void NetworkObserverEventUnittest::TestConnIdFromConnectId() {
    connect_id_t conn_id;
    conn_id.fd = 5;
    conn_id.tgid = 2000;
    conn_id.start = 789012;

    ConnId id(conn_id);
    APSARA_TEST_EQUAL(id.fd, 5);
    APSARA_TEST_EQUAL(id.tgid, 2000);
    APSARA_TEST_EQUAL(id.start, 789012);
}

void NetworkObserverEventUnittest::TestHeadersMapCaseInsensitive() {
    HeadersMap headers;
    headers.insert({"Content-Type", "application/json"});

    // 测试大小写不敏感的键查找
    auto res = headers.find("content-type");
    APSARA_TEST_NOT_EQUAL(res, headers.end());
    APSARA_TEST_STREQ(res->second.c_str(), "application/json");

    res = headers.find("CONTENT-TYPE");
    APSARA_TEST_NOT_EQUAL(res, headers.end());
    APSARA_TEST_STREQ(res->second.c_str(), "application/json");

    // 测试多值插入
    headers.insert({"Accept", "text/plain"});
    headers.insert({"ACCEPT", "application/xml"});
    APSARA_TEST_EQUAL(headers.size(), 3);

    // 测试不存在的键
    APSARA_TEST_TRUE(headers.find("nonexistent") == headers.end());
}

void NetworkObserverEventUnittest::TestHttpRecordTimestamps() {
    ConnId id(1, 1000, 123456);
    HttpRecord record(std::move(id));

    record.SetStartTs(1000000);
    record.SetEndTs(2000000);

    APSARA_TEST_EQUAL(record.GetStartTimeStamp(), 1000000);
    APSARA_TEST_EQUAL(record.GetEndTimeStamp(), 2000000);
    APSARA_TEST_EQUAL(record.GetLatencyNs(), 1000000);
    APSARA_TEST_EQUAL(record.GetLatencyMs(), 1000);
}

void NetworkObserverEventUnittest::TestHttpRecordStatus() {
    ConnId id(1, 1000, 123456);
    HttpRecord record(std::move(id));

    // 测试正常状态码
    record.SetStatusCode("200");
    APSARA_TEST_FALSE(record.IsError());

    // 测试错误状态码
    record.SetStatusCode("500");
    APSARA_TEST_TRUE(record.IsError());

    // 测试慢请求判断
    record.SetStartTs(0);
    record.SetEndTs(600000000); // 600ms
    APSARA_TEST_TRUE(record.IsSlow());

    HeadersMap reqHeaders;
    reqHeaders.insert({"Content-Type", "application/json"});
    record.SetReqHeaderMap(reqHeaders);

    HeadersMap respHeaders;
    respHeaders.insert({"Content-Length", "100"});
    record.SetRespHeaderMap(respHeaders);

    APSARA_TEST_EQUAL(record.GetReqHeaderMap().size(), 1);
    APSARA_TEST_EQUAL(record.GetRespHeaderMap().size(), 1);
}

void NetworkObserverEventUnittest::TestAbstractNetRecord() {
    ConnId id(1, 1000, 123456);
    ConnStatsRecord record(id);

    // 测试基类功能
    APSARA_TEST_EQUAL(record.GetAggregateType(), AggregateType::NETWORK);
    APSARA_TEST_TRUE(record.GetConnId() == id);

    // 测试表模式
    APSARA_TEST_TRUE(record.GetMetricsTableSchema().name() == "net_metrics");
    APSARA_TEST_TRUE(record.GetTableSchema().name() == "net_record");
}

UNIT_TEST_CASE(NetworkObserverEventUnittest, TestConnId);
UNIT_TEST_CASE(NetworkObserverEventUnittest, TestConnIdHash);
UNIT_TEST_CASE(NetworkObserverEventUnittest, TestCaseInsensitiveLess);
UNIT_TEST_CASE(NetworkObserverEventUnittest, TestHeadersMap);
UNIT_TEST_CASE(NetworkObserverEventUnittest, TestConnStatsRecord);
UNIT_TEST_CASE(NetworkObserverEventUnittest, TestHttpRecord);
UNIT_TEST_CASE(NetworkObserverEventUnittest, TestAppMetricData);
UNIT_TEST_CASE(NetworkObserverEventUnittest, TestNetMetricData);
UNIT_TEST_CASE(NetworkObserverEventUnittest, TestConnIdMove);
UNIT_TEST_CASE(NetworkObserverEventUnittest, TestConnIdFromConnectId);
UNIT_TEST_CASE(NetworkObserverEventUnittest, TestHeadersMapCaseInsensitive);
UNIT_TEST_CASE(NetworkObserverEventUnittest, TestHttpRecordTimestamps);
UNIT_TEST_CASE(NetworkObserverEventUnittest, TestHttpRecordStatus);
UNIT_TEST_CASE(NetworkObserverEventUnittest, TestAbstractNetRecord);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
