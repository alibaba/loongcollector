/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "host_monitor/common/FastFieldParser.h"
#include "unittest/Unittest.h"

using namespace std;
using namespace logtail;

namespace logtail {

class FastFieldParserUnittest : public ::testing::Test {
public:
    void TestBasicFieldAccess();
    void TestNumericParsing();
    void TestCpuStatParser();
    void TestNetDevParser();
    void TestEdgeCases();
    void TestFastParseNamespace();

protected:
    void SetUp() override {
        // 准备测试数据
    }
};

void FastFieldParserUnittest::TestBasicFieldAccess() {
    // 测试基础字段访问功能
    string testLine = "field0 field1 field2 field3 field4";
    FastFieldParser parser(testLine);

    // 测试按索引获取字段
    APSARA_TEST_EQUAL("field0", string(parser.GetField(0)));
    APSARA_TEST_EQUAL("field1", string(parser.GetField(1)));
    APSARA_TEST_EQUAL("field2", string(parser.GetField(2)));
    APSARA_TEST_EQUAL("field3", string(parser.GetField(3)));
    APSARA_TEST_EQUAL("field4", string(parser.GetField(4)));

    // 测试越界访问
    APSARA_TEST_TRUE(parser.GetField(5).empty());
    APSARA_TEST_TRUE(parser.GetField(100).empty());

    // 测试字段总数
    APSARA_TEST_EQUAL(5U, parser.GetFieldCount());

    // 测试前缀匹配
    APSARA_TEST_TRUE(parser.FieldStartsWith(0, "field"));
    APSARA_TEST_TRUE(parser.FieldStartsWith(1, "field1"));
    APSARA_TEST_FALSE(parser.FieldStartsWith(0, "wrong"));

    // 测试批量获取字段（使用迭代器）
    vector<StringView> fields;
    auto iter = parser.begin();
    auto end = parser.end();
    ++iter; // 跳过第一个字段
    for (size_t i = 0; i < 3 && iter != end; ++i, ++iter) {
        fields.push_back(*iter);
    }
    APSARA_TEST_EQUAL(3U, fields.size());
    APSARA_TEST_EQUAL("field1", string(fields[0]));
    APSARA_TEST_EQUAL("field2", string(fields[1]));
    APSARA_TEST_EQUAL("field3", string(fields[2]));
}

void FastFieldParserUnittest::TestNumericParsing() {
    // 测试数值解析功能
    string testLine = "123 456.78 -789 0 999999999999";
    FastFieldParser parser(testLine);

    // 测试整数解析
    APSARA_TEST_EQUAL(123, parser.GetFieldAs<int>(0));
    APSARA_TEST_EQUAL(-789, parser.GetFieldAs<int>(2));
    APSARA_TEST_EQUAL(0, parser.GetFieldAs<int>(3));
    APSARA_TEST_EQUAL(999999999999ULL, parser.GetFieldAs<uint64_t>(4));

    // 测试浮点数解析
    APSARA_TEST_EQUAL(456.78, parser.GetFieldAs<double>(1));
    APSARA_TEST_EQUAL(-789.0, parser.GetFieldAs<double>(2));

    // 测试默认值
    APSARA_TEST_EQUAL(42, parser.GetFieldAs<int>(10, 42));
    APSARA_TEST_EQUAL(3.14, parser.GetFieldAs<double>(10, 3.14));

    // 测试无效数据
    string invalidLine = "abc def ghi";
    FastFieldParser invalidParser(invalidLine);
    APSARA_TEST_EQUAL(0, invalidParser.GetFieldAs<int>(0, 0));
    APSARA_TEST_EQUAL(999, invalidParser.GetFieldAs<int>(0, 999));
}

void FastFieldParserUnittest::TestCpuStatParser() {
    // 测试CPU统计专用解析器

    // 测试总体CPU行
    string cpuTotalLine = "cpu  1195061569 1728645 418424132 203670447952 14723544 0 773400 0 0 0";
    CpuStatParser totalParser(cpuTotalLine);

    APSARA_TEST_TRUE(totalParser.IsCpuLine());
    APSARA_TEST_EQUAL(-1, totalParser.GetCpuIndex());

    double user, nice, system, idle, iowait, irq, softirq, steal, guest, guestNice;
    totalParser.GetCpuStats(user, nice, system, idle, iowait, irq, softirq, steal, guest, guestNice);

    APSARA_TEST_EQUAL(1195061569.0, user);
    APSARA_TEST_EQUAL(1728645.0, nice);
    APSARA_TEST_EQUAL(418424132.0, system);
    APSARA_TEST_EQUAL(203670447952.0, idle);

    // 测试具体CPU核心行
    string cpu0Line = "cpu0 14708487 14216 4613031 2108180843 57199 0 424744 0 0 0";
    CpuStatParser cpu0Parser(cpu0Line);

    APSARA_TEST_TRUE(cpu0Parser.IsCpuLine());
    APSARA_TEST_EQUAL(0, cpu0Parser.GetCpuIndex());

    // 测试非CPU行
    string nonCpuLine = "intr 123456789 0 0 0";
    CpuStatParser nonCpuParser(nonCpuLine);

    APSARA_TEST_FALSE(nonCpuParser.IsCpuLine());
}

void FastFieldParserUnittest::TestNetDevParser() {
    // 测试网络设备统计解析器
    string netDevLine = "  eth0: 1234567890 123456 0 0 0 0 0 0 987654321 98765 0 0 0 0 0 0";

    NetDevParser parser(netDevLine);
    StringView deviceName;
    vector<uint64_t> stats;

    APSARA_TEST_TRUE(parser.ParseDeviceStats(deviceName, stats));
    APSARA_TEST_EQUAL("eth0", string(deviceName));
    APSARA_TEST_EQUAL(16U, stats.size());

    // 验证一些关键统计值
    APSARA_TEST_EQUAL(1234567890ULL, stats[0]); // rx_bytes
    APSARA_TEST_EQUAL(123456ULL, stats[1]); // rx_packets
    APSARA_TEST_EQUAL(987654321ULL, stats[8]); // tx_bytes
    APSARA_TEST_EQUAL(98765ULL, stats[9]); // tx_packets

    // 测试无效行
    string invalidLine = "invalid line without colon";
    NetDevParser invalidParser(invalidLine);

    StringView invalidName;
    vector<uint64_t> invalidStats;
    APSARA_TEST_FALSE(invalidParser.ParseDeviceStats(invalidName, invalidStats));
}

void FastFieldParserUnittest::TestEdgeCases() {
    // 测试边界情况

    // 空字符串
    FastFieldParser emptyParser("");
    APSARA_TEST_EQUAL(0U, emptyParser.GetFieldCount());
    APSARA_TEST_TRUE(emptyParser.GetField(0).empty());

    // 只有分隔符
    FastFieldParser spaceParser("   ");
    APSARA_TEST_EQUAL(0U, spaceParser.GetFieldCount());

    // 单个字段
    FastFieldParser singleParser("single");
    APSARA_TEST_EQUAL(1U, singleParser.GetFieldCount());
    APSARA_TEST_EQUAL("single", string(singleParser.GetField(0)));

    // 连续分隔符
    FastFieldParser multiSpaceParser("field1   field2    field3");
    APSARA_TEST_EQUAL(3U, multiSpaceParser.GetFieldCount());
    APSARA_TEST_EQUAL("field1", string(multiSpaceParser.GetField(0)));
    APSARA_TEST_EQUAL("field2", string(multiSpaceParser.GetField(1)));
    APSARA_TEST_EQUAL("field3", string(multiSpaceParser.GetField(2)));

    // 自定义分隔符
    FastFieldParser csvParser("a,b,c,d", ',');
    APSARA_TEST_EQUAL(4U, csvParser.GetFieldCount());
    APSARA_TEST_EQUAL("a", string(csvParser.GetField(0)));
    APSARA_TEST_EQUAL("b", string(csvParser.GetField(1)));
    APSARA_TEST_EQUAL("c", string(csvParser.GetField(2)));
    APSARA_TEST_EQUAL("d", string(csvParser.GetField(3)));

    // 前后有分隔符
    FastFieldParser trimParser(" field1 field2 ");
    APSARA_TEST_EQUAL(2U, trimParser.GetFieldCount());
    APSARA_TEST_EQUAL("field1", string(trimParser.GetField(0)));
    APSARA_TEST_EQUAL("field2", string(trimParser.GetField(1)));
}

void FastFieldParserUnittest::TestFastParseNamespace() {
    // 测试便利函数命名空间
    string testLine = "100 200.5 text 300";

    // 测试快速字段获取
    APSARA_TEST_EQUAL("100", string(FastParse::GetField(testLine, 0)));
    APSARA_TEST_EQUAL("200.5", string(FastParse::GetField(testLine, 1)));
    APSARA_TEST_EQUAL("text", string(FastParse::GetField(testLine, 2)));

    // 测试快速数值解析
    APSARA_TEST_EQUAL(100, FastParse::GetFieldAs<int>(testLine, 0));
    APSARA_TEST_EQUAL(200.5, FastParse::GetFieldAs<double>(testLine, 1));
    APSARA_TEST_EQUAL(300ULL, FastParse::GetFieldAs<uint64_t>(testLine, 3));

    // 测试前缀检查
    APSARA_TEST_TRUE(FastParse::FieldStartsWith(testLine, 2, "tex"));
    APSARA_TEST_FALSE(FastParse::FieldStartsWith(testLine, 2, "wrong"));

    // 测试自定义分隔符
    string csvLine = "a,b,c";
    APSARA_TEST_EQUAL("a", string(FastParse::GetField(csvLine, 0, ',')));
    APSARA_TEST_EQUAL("b", string(FastParse::GetField(csvLine, 1, ',')));
    APSARA_TEST_EQUAL("c", string(FastParse::GetField(csvLine, 2, ',')));
}

// 注册测试用例
TEST_F(FastFieldParserUnittest, BasicFieldAccess) {
    TestBasicFieldAccess();
}

TEST_F(FastFieldParserUnittest, NumericParsing) {
    TestNumericParsing();
}

TEST_F(FastFieldParserUnittest, CpuStatParser) {
    TestCpuStatParser();
}

TEST_F(FastFieldParserUnittest, NetDevParser) {
    TestNetDevParser();
}

TEST_F(FastFieldParserUnittest, EdgeCases) {
    TestEdgeCases();
}

TEST_F(FastFieldParserUnittest, FastParseNamespace) {
    TestFastParseNamespace();
}

} // namespace logtail

UNIT_TEST_MAIN
