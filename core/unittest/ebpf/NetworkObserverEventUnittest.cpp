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
    std::shared_ptr<Connection> CreateTestTracker() {
        ConnId connId(1, 1000, 123456);
        return std::make_shared<Connection>(connId);
    }
    void SetUp() override {}
    void TearDown() override {}
};

void NetworkObserverEventUnittest::TestConnId() {
    // 测试构造函数
    ConnId id1(1, 1000, 123456);
    APSARA_TEST_EQUAL(id1.fd, 1);
    APSARA_TEST_EQUAL(id1.tgid, 1000U);
    APSARA_TEST_EQUAL(id1.start, 123456UL);

    // 测试拷贝构造
    ConnId id2(id1);
    APSARA_TEST_EQUAL(id2.fd, id1.fd);
    APSARA_TEST_EQUAL(id2.tgid, id1.tgid);
    APSARA_TEST_EQUAL(id2.start, id1.start);

    // 测试移动构造
    ConnId id3(ConnId(2, 2000, 234567));
    APSARA_TEST_EQUAL(id3.fd, 2);
    APSARA_TEST_EQUAL(id3.tgid, 2000U);
    APSARA_TEST_EQUAL(id3.start, 234567UL);

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

    APSARA_TEST_EQUAL(values.size(), 2UL);
    APSARA_TEST_TRUE(std::find(values.begin(), values.end(), "application/json") != values.end());
    APSARA_TEST_TRUE(std::find(values.begin(), values.end(), "text/plain") != values.end());
}

void NetworkObserverEventUnittest::TestConnStatsRecord() {
    // ConnId id(1, 1000, 123456);
    auto conn = CreateTestTracker();

    ConnStatsRecord record(conn);

    // 测试基本属性
    APSARA_TEST_FALSE(record.IsError());
    APSARA_TEST_FALSE(record.IsSlow());
    APSARA_TEST_EQUAL(record.GetStatusCode(), 0);
}

void NetworkObserverEventUnittest::TestHttpRecord() {
    auto conn = CreateTestTracker();
    HttpRecord record(conn);

    record.SetPath("/api/v1/test");
    record.SetMethod("GET");
    record.SetStatusCode(200);
    record.SetProtocolVersion("HTTP/1.1");

    APSARA_TEST_EQUAL(record.GetPath(), "/api/v1/test");
    APSARA_TEST_EQUAL(record.GetMethod(), "GET");
    APSARA_TEST_EQUAL(record.GetStatusCode(), 200);
    APSARA_TEST_EQUAL(record.GetProtocolVersion(), "HTTP/1.1");
    APSARA_TEST_FALSE(record.IsError());

    record.SetStatusCode(404);
    APSARA_TEST_TRUE(record.IsError());
    APSARA_TEST_EQUAL(record.GetStatusCode(), 404);

    // 测试请求头
    HeadersMap reqHeaders;
    reqHeaders.insert({"Content-Type", "application/json"});
    record.SetReqHeaderMap(std::move(reqHeaders));
    APSARA_TEST_EQUAL(record.GetReqHeaderMap().size(), 1UL);

    // 测试响应头
    HeadersMap respHeaders;
    respHeaders.insert({"Content-Length", "100"});
    record.SetRespHeaderMap(std::move(respHeaders));
    APSARA_TEST_EQUAL(record.GetRespHeaderMap().size(), 1UL);
}

void NetworkObserverEventUnittest::TestAppMetricData() {
    auto conn = CreateTestTracker();
    std::shared_ptr<SourceBuffer> sourceBuffer = std::make_shared<SourceBuffer>();
    AppMetricData data(conn, sourceBuffer, "test_span");

    // 测试基本属性设置和获取
    data.mCount = 100;
    data.mSum = 1000.0;
    data.mSlowCount = 5;
    data.mErrCount = 2;
    data.m2xxCount = 80;
    data.m3xxCount = 10;
    data.m4xxCount = 8;
    data.m5xxCount = 2;

    APSARA_TEST_EQUAL(data.mCount, 100UL);
    APSARA_TEST_EQUAL(data.mSum, 1000.0);
    APSARA_TEST_EQUAL(data.mSlowCount, 5UL);
    APSARA_TEST_EQUAL(data.mErrCount, 2UL);
    APSARA_TEST_EQUAL(data.m2xxCount, 80UL);
    APSARA_TEST_EQUAL(data.m3xxCount, 10UL);
    APSARA_TEST_EQUAL(data.m4xxCount, 8UL);
    APSARA_TEST_EQUAL(data.m5xxCount, 2UL);
}

void NetworkObserverEventUnittest::TestNetMetricData() {
    auto conn = CreateTestTracker();
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    NetMetricData data(conn, sourceBuffer);

    // 测试基本属性设置和获取
    data.mDropCount = 10;
    data.mRetransCount = 5;
    data.mRtt = 100;
    data.mRecvBytes = 1024;
    data.mSendBytes = 2048;
    data.mRecvPkts = 100;
    data.mSendPkts = 200;

    APSARA_TEST_EQUAL(data.mDropCount, 10UL);
    APSARA_TEST_EQUAL(data.mRetransCount, 5UL);
    APSARA_TEST_EQUAL(data.mRtt, 100UL);
    APSARA_TEST_EQUAL(data.mRecvBytes, 1024UL);
    APSARA_TEST_EQUAL(data.mSendBytes, 2048UL);
    APSARA_TEST_EQUAL(data.mRecvPkts, 100UL);
    APSARA_TEST_EQUAL(data.mSendPkts, 200UL);
}

void NetworkObserverEventUnittest::TestConnIdMove() {
    ConnId id1(1, 1000, 123456);
    ConnId id2(std::move(id1));
    APSARA_TEST_EQUAL(id2.fd, 1);
    APSARA_TEST_EQUAL(id2.tgid, 1000U);
    APSARA_TEST_EQUAL(id2.start, 123456UL);
}

void NetworkObserverEventUnittest::TestConnIdFromConnectId() {
    connect_id_t conn_id;
    conn_id.fd = 5;
    conn_id.tgid = 2000;
    conn_id.start = 789012;

    ConnId id(conn_id);
    APSARA_TEST_EQUAL(id.fd, 5);
    APSARA_TEST_EQUAL(id.tgid, 2000U);
    APSARA_TEST_EQUAL(id.start, 789012UL);
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
    APSARA_TEST_EQUAL(headers.size(), 3UL);

    // 测试不存在的键
    APSARA_TEST_TRUE(headers.find("nonexistent") == headers.end());
}

void NetworkObserverEventUnittest::TestHttpRecordTimestamps() {
    auto conn = CreateTestTracker();
    HttpRecord record(conn);

    record.SetStartTsNs(1000000);
    record.SetEndTsNs(2000000);

    APSARA_TEST_EQUAL(record.GetStartTimeStamp(), 1000000UL);
    APSARA_TEST_EQUAL(record.GetEndTimeStamp(), 2000000UL);
    APSARA_TEST_EQUAL(record.GetLatencyNs(), 1000000.0);
    APSARA_TEST_EQUAL(record.GetLatencyMs(), 1.0);
}

void NetworkObserverEventUnittest::TestHttpRecordStatus() {
    auto conn = CreateTestTracker();
    HttpRecord record(conn);

    record.SetStatusCode(200);
    APSARA_TEST_FALSE(record.IsError());

    record.SetStatusCode(500);
    APSARA_TEST_TRUE(record.IsError());

    record.SetStartTsNs(0);
    record.SetEndTsNs(600000000); // 600ms
    APSARA_TEST_TRUE(record.IsSlow());

    HeadersMap reqHeaders;
    reqHeaders.insert({"Content-Type", "application/json"});
    record.SetReqHeaderMap(std::move(reqHeaders));

    HeadersMap respHeaders;
    respHeaders.insert({"Content-Length", "100"});
    record.SetRespHeaderMap(std::move(respHeaders));

    APSARA_TEST_EQUAL(record.GetReqHeaderMap().size(), 1UL);
    APSARA_TEST_EQUAL(record.GetRespHeaderMap().size(), 1UL);
}

void NetworkObserverEventUnittest::TestAbstractNetRecord() {
    auto conn = CreateTestTracker();
    ConnStatsRecord record(conn);
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
