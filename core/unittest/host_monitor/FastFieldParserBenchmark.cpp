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

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "host_monitor/common/FastFieldParser.h"
#include "unittest/Unittest.h"

using namespace std;
using namespace logtail;

namespace logtail {

class FastFieldParserBenchmark : public ::testing::Test {
public:
    void TestCPUStatParsing();
    void TestSocketStatParsing();
    void TestLoadStatParsing();
    void TestNetDevParsing();
    void TestFieldAccess();
    void TestProcessCredParsing();
    void TestIPv6InterfaceParsing();
    void TestUptimeParsing();
    void TestDiskStateParsing();
    void TestBatchFieldParsing();
    void TestProcessMemoryParsing();
    void TestProcessArgsParsing();

protected:
    void SetUp() override {
        // 准备测试数据 - 模拟真实的系统监控数据
        cpuTestLines = {"cpu  1195061569 1728645 418424132 203670447952 14723544 0 773400 0 0 0",
                        "cpu0 14708487 14216 4613031 2108180843 57199 0 424744 0 0 0",
                        "cpu1 15234567 15432 4987654 2234567890 58765 0 456789 0 0 0",
                        "cpu2 16345678 16543 5098765 2345678901 59876 0 567890 0 0 0",
                        "cpu3 17456789 17654 6109876 2456789012 60987 0 678901 0 0 0",
                        "cpu4 18567890 18765 7210987 2567890123 61098 0 789012 0 0 0",
                        "cpu5 19678901 19876 8321098 2678901234 62109 0 890123 0 0 0",
                        "cpu6 20789012 20987 9432109 2789012345 63210 0 901234 0 0 0",
                        "cpu7 21890123 21098 10543210 2890123456 64321 0 012345 0 0 0"};

        sockStatTestLines = {"sockets: used 316",
                             "TCP: inuse 25 orphan 0 tw 2 alloc 28 mem 4",
                             "UDP: inuse 3 mem 0",
                             "UDPLITE: inuse 0",
                             "RAW: inuse 0",
                             "FRAG: inuse 0 memory 0",
                             "TCP6: inuse 15 orphan 0 tw 0 alloc 67 mem 234"};

        loadTestLines = {"0.10 0.07 0.03 1/561 78450"};

        netDevTestLines = {"  eth0: 1234567890 123456 0 0 0 0 0 0 987654321 98765 0 0 0 0 0 0",
                           "  lo: 987654321 98765 0 0 0 0 0 0 987654321 98765 0 0 0 0 0 0",
                           "  wlan0: 555666777 55566 1 2 3 4 5 6 444333222 44433 7 8 9 10 11 12"};

        benchmarkIterations = 50000; // 5万次迭代，在CI环境中较快
    }

    // 测试数据
    vector<string> cpuTestLines;
    vector<string> sockStatTestLines;
    vector<string> loadTestLines;
    vector<string> netDevTestLines;
    int benchmarkIterations;

private:
    // 用于结果验证的结构体
    struct CPUStatResult {
        int index;
        double user, nice, system, idle, iowait, irq, softirq, steal, guest, guestNice;
    };

    // Boost::split版本的CPU解析（用于对比）
    CPUStatResult ParseCPUWithBoost(const string& line) {
        CPUStatResult result{};
        vector<string> cpuMetric;
        boost::split(cpuMetric, line, boost::is_any_of(" "), boost::token_compress_on);

        if (cpuMetric.size() > 0 && cpuMetric[0].substr(0, 3) == "cpu") {
            if (cpuMetric[0] == "cpu") {
                result.index = -1;
            } else {
                string indexStr = cpuMetric[0].substr(3);
                result.index = indexStr.empty() ? -1 : stoi(indexStr);
            }

            result.user = cpuMetric.size() > 1 ? stod(cpuMetric[1]) : 0.0;
            result.nice = cpuMetric.size() > 2 ? stod(cpuMetric[2]) : 0.0;
            result.system = cpuMetric.size() > 3 ? stod(cpuMetric[3]) : 0.0;
            result.idle = cpuMetric.size() > 4 ? stod(cpuMetric[4]) : 0.0;
            result.iowait = cpuMetric.size() > 5 ? stod(cpuMetric[5]) : 0.0;
            result.irq = cpuMetric.size() > 6 ? stod(cpuMetric[6]) : 0.0;
            result.softirq = cpuMetric.size() > 7 ? stod(cpuMetric[7]) : 0.0;
            result.steal = cpuMetric.size() > 8 ? stod(cpuMetric[8]) : 0.0;
            result.guest = cpuMetric.size() > 9 ? stod(cpuMetric[9]) : 0.0;
            result.guestNice = cpuMetric.size() > 10 ? stod(cpuMetric[10]) : 0.0;
        }

        return result;
    }

    // FastFieldParser版本的CPU解析
    CPUStatResult ParseCPUWithFast(const string& line) {
        CPUStatResult result{};
        CpuStatParser parser(line);

        if (parser.IsCpuLine()) {
            result.index = parser.GetCpuIndex();
            parser.GetCpuStats(result.user,
                               result.nice,
                               result.system,
                               result.idle,
                               result.iowait,
                               result.irq,
                               result.softirq,
                               result.steal,
                               result.guest,
                               result.guestNice);
        }

        return result;
    }

    // Socket统计解析（Boost版本）
    uint64_t ParseSocketWithBoost(const vector<string>& lines) {
        uint64_t tcp = 0;
        for (const auto& line : lines) {
            if (line.size() >= 5 && (line.substr(0, 4) == "TCP:" || line.substr(0, 5) == "TCP6:")) {
                vector<string> metrics;
                boost::split(metrics, line, boost::is_any_of(" "), boost::token_compress_on);
                if (metrics.size() >= 9) {
                    tcp += static_cast<uint64_t>(stoull(metrics[6])); // tw
                    tcp += static_cast<uint64_t>(stoull(metrics[8])); // alloc
                }
            }
        }
        return tcp;
    }

    // Socket统计解析（Fast版本）
    uint64_t ParseSocketWithFast(const vector<string>& lines) {
        uint64_t tcp = 0;
        for (const auto& line : lines) {
            if (FastParse::FieldStartsWith(line, 0, "TCP:") || FastParse::FieldStartsWith(line, 0, "TCP6:")) {
                auto twValue = FastParse::GetFieldAs<uint64_t>(line, 6, 0);
                auto allocValue = FastParse::GetFieldAs<uint64_t>(line, 8, 0);
                tcp += twValue + allocValue;
            }
        }
        return tcp;
    }
};

void FastFieldParserBenchmark::TestCPUStatParsing() {
    cout << "\n=== CPU统计解析性能测试 ===\n";

    // 验证结果正确性
    {
        auto boostResult = ParseCPUWithBoost(cpuTestLines[0]);
        auto fastResult = ParseCPUWithFast(cpuTestLines[0]);

        APSARA_TEST_EQUAL(-1, boostResult.index);
        APSARA_TEST_EQUAL(-1, fastResult.index);
        APSARA_TEST_EQUAL(boostResult.user, fastResult.user);
        APSARA_TEST_EQUAL(boostResult.nice, fastResult.nice);
        APSARA_TEST_EQUAL(boostResult.system, fastResult.system);

        cout << "✅ 结果正确性验证通过\n";
    }

    // 性能测试 - Boost::split版本
    auto start = chrono::high_resolution_clock::now();

    volatile int boostChecksum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        for (const auto& line : cpuTestLines) {
            auto result = ParseCPUWithBoost(line);
            boostChecksum += result.index; // 防止编译器优化
        }
    }

    auto boostEnd = chrono::high_resolution_clock::now();

    // 性能测试 - FastFieldParser版本
    volatile int fastChecksum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        for (const auto& line : cpuTestLines) {
            auto result = ParseCPUWithFast(line);
            fastChecksum += result.index; // 防止编译器优化
        }
    }

    auto fastEnd = chrono::high_resolution_clock::now();

    // 计算和输出结果
    auto boostTime = chrono::duration_cast<chrono::microseconds>(boostEnd - start);
    auto fastTime = chrono::duration_cast<chrono::microseconds>(fastEnd - boostEnd);

    double speedup = static_cast<double>(boostTime.count()) / fastTime.count();

    cout << "CPU统计解析 (" << benchmarkIterations << " 次迭代, " << cpuTestLines.size() << " 行/次):\n";
    cout << "  Boost::split:    " << boostTime.count() << " μs\n";
    cout << "  FastFieldParser: " << fastTime.count() << " μs\n";
    cout << "  加速比:          " << fixed << setprecision(2) << speedup << "x\n";
    cout << "  校验和:          " << boostChecksum << " vs " << fastChecksum << "\n";

    APSARA_TEST_EQUAL(boostChecksum, fastChecksum); // 结果应该一致
}

void FastFieldParserBenchmark::TestSocketStatParsing() {
    cout << "\n=== Socket统计解析性能测试 ===\n";

    // 验证结果正确性
    {
        auto boostResult = ParseSocketWithBoost(sockStatTestLines);
        auto fastResult = ParseSocketWithFast(sockStatTestLines);

        APSARA_TEST_EQUAL(boostResult, fastResult);
        cout << "✅ 结果正确性验证通过 (结果: " << boostResult << ")\n";
    }

    // 性能测试
    auto start = chrono::high_resolution_clock::now();

    volatile uint64_t boostSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        boostSum += ParseSocketWithBoost(sockStatTestLines);
    }

    auto boostEnd = chrono::high_resolution_clock::now();

    volatile uint64_t fastSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        fastSum += ParseSocketWithFast(sockStatTestLines);
    }

    auto fastEnd = chrono::high_resolution_clock::now();

    auto boostTime = chrono::duration_cast<chrono::microseconds>(boostEnd - start);
    auto fastTime = chrono::duration_cast<chrono::microseconds>(fastEnd - boostEnd);

    double speedup = static_cast<double>(boostTime.count()) / fastTime.count();

    cout << "Socket统计解析 (" << benchmarkIterations << " 次迭代):\n";
    cout << "  Boost::split:    " << boostTime.count() << " μs\n";
    cout << "  FastFieldParser: " << fastTime.count() << " μs\n";
    cout << "  加速比:          " << fixed << setprecision(2) << speedup << "x\n";
    cout << "  校验和:          " << boostSum << " vs " << fastSum << "\n";

    APSARA_TEST_EQUAL(boostSum, fastSum);
}

void FastFieldParserBenchmark::TestLoadStatParsing() {
    cout << "\n=== 系统负载解析性能测试 ===\n";

    const string& loadLine = loadTestLines[0];

    // Boost版本
    auto parseLoadBoost = [&]() -> vector<double> {
        vector<string> loadMetric;
        boost::split(loadMetric, loadLine, boost::is_any_of(" "), boost::token_compress_on);

        vector<double> result;
        if (loadMetric.size() >= 3) {
            result.push_back(stod(loadMetric[0]));
            result.push_back(stod(loadMetric[1]));
            result.push_back(stod(loadMetric[2]));
        }
        return result;
    };

    // Fast版本
    auto parseLoadFast = [&]() -> vector<double> {
        vector<double> result;
        result.push_back(FastParse::GetFieldAs<double>(loadLine, 0, 0.0));
        result.push_back(FastParse::GetFieldAs<double>(loadLine, 1, 0.0));
        result.push_back(FastParse::GetFieldAs<double>(loadLine, 2, 0.0));
        return result;
    };

    // 验证正确性
    {
        auto boostResult = parseLoadBoost();
        auto fastResult = parseLoadFast();

        APSARA_TEST_EQUAL(boostResult.size(), fastResult.size());
        for (size_t i = 0; i < boostResult.size(); ++i) {
            APSARA_TEST_EQUAL(boostResult[i], fastResult[i]);
        }
        cout << "✅ 结果正确性验证通过\n";
    }

    // 性能测试
    auto start = chrono::high_resolution_clock::now();

    volatile double boostSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        auto result = parseLoadBoost();
        boostSum += result[0] + result[1] + result[2];
    }

    auto boostEnd = chrono::high_resolution_clock::now();

    volatile double fastSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        auto result = parseLoadFast();
        fastSum += result[0] + result[1] + result[2];
    }

    auto fastEnd = chrono::high_resolution_clock::now();

    auto boostTime = chrono::duration_cast<chrono::microseconds>(boostEnd - start);
    auto fastTime = chrono::duration_cast<chrono::microseconds>(fastEnd - boostEnd);

    double speedup = static_cast<double>(boostTime.count()) / fastTime.count();

    cout << "系统负载解析 (" << benchmarkIterations << " 次迭代):\n";
    cout << "  Boost::split:    " << boostTime.count() << " μs\n";
    cout << "  FastFieldParser: " << fastTime.count() << " μs\n";
    cout << "  加速比:          " << fixed << setprecision(2) << speedup << "x\n";
}

void FastFieldParserBenchmark::TestNetDevParsing() {
    cout << "\n=== 网络设备统计解析性能测试 ===\n";

    // Boost版本网络设备解析
    auto parseNetDevBoost = [](const string& line) -> pair<string, vector<uint64_t>> {
        auto pos = line.find_first_of(':');
        if (pos == string::npos) {
            return {};
        }

        string devCounterStr = line.substr(pos + 1);
        string devName = line.substr(0, pos);

        boost::algorithm::trim(devName);
        boost::algorithm::trim(devCounterStr);

        vector<string> netDevMetric;
        boost::split(netDevMetric, devCounterStr, boost::is_any_of(" "), boost::token_compress_on);

        vector<uint64_t> stats;
        for (const auto& metric : netDevMetric) {
            if (!metric.empty()) {
                stats.push_back(static_cast<uint64_t>(stoull(metric)));
            }
        }

        return {devName, stats};
    };

    // Fast版本网络设备解析
    auto parseNetDevFast = [](const string& line) -> pair<string, vector<uint64_t>> {
        NetDevParser parser(line);
        string_view deviceNameView;
        vector<uint64_t> stats;

        if (parser.ParseDeviceStats(deviceNameView, stats)) {
            return {string(deviceNameView), stats};
        }

        return {};
    };

    // 验证正确性
    {
        for (const auto& line : netDevTestLines) {
            auto boostResult = parseNetDevBoost(line);
            auto fastResult = parseNetDevFast(line);

            APSARA_TEST_EQUAL(boostResult.first, fastResult.first);
            APSARA_TEST_EQUAL(boostResult.second.size(), fastResult.second.size());

            for (size_t i = 0; i < boostResult.second.size(); ++i) {
                APSARA_TEST_EQUAL(boostResult.second[i], fastResult.second[i]);
            }
        }
        cout << "✅ 结果正确性验证通过\n";
    }

    // 性能测试
    auto start = chrono::high_resolution_clock::now();

    volatile uint64_t boostSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        for (const auto& line : netDevTestLines) {
            auto result = parseNetDevBoost(line);
            for (auto val : result.second) {
                boostSum += val;
            }
        }
    }

    auto boostEnd = chrono::high_resolution_clock::now();

    volatile uint64_t fastSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        for (const auto& line : netDevTestLines) {
            auto result = parseNetDevFast(line);
            for (auto val : result.second) {
                fastSum += val;
            }
        }
    }

    auto fastEnd = chrono::high_resolution_clock::now();

    auto boostTime = chrono::duration_cast<chrono::microseconds>(boostEnd - start);
    auto fastTime = chrono::duration_cast<chrono::microseconds>(fastEnd - boostEnd);

    double speedup = static_cast<double>(boostTime.count()) / fastTime.count();

    cout << "网络设备解析 (" << benchmarkIterations << " 次迭代, " << netDevTestLines.size() << " 行/次):\n";
    cout << "  Boost::split:    " << boostTime.count() << " μs\n";
    cout << "  FastFieldParser: " << fastTime.count() << " μs\n";
    cout << "  加速比:          " << fixed << setprecision(2) << speedup << "x\n";
    cout << "  校验和:          " << boostSum << " vs " << fastSum << "\n";

    APSARA_TEST_EQUAL(boostSum, fastSum);
}

void FastFieldParserBenchmark::TestFieldAccess() {
    cout << "\n=== 单字段访问性能测试 ===\n";

    const string testLine = "field0 field1 field2 field3 field4 field5 field6 field7 field8 field9";

    // Boost版本 - 分割后访问
    auto accessBoost = [&](size_t index) -> string {
        vector<string> fields;
        boost::split(fields, testLine, boost::is_any_of(" "), boost::token_compress_on);
        return index < fields.size() ? fields[index] : "";
    };

    // Fast版本 - 直接访问
    auto accessFast = [&](size_t index) -> string_view { return FastParse::GetField(testLine, index); };

    // 验证正确性
    for (size_t i = 0; i < 10; ++i) {
        auto boostResult = accessBoost(i);
        auto fastResult = accessFast(i);
        APSARA_TEST_EQUAL(boostResult, string(fastResult));
    }
    cout << "✅ 结果正确性验证通过\n";

    // 性能测试 - 随机访问不同字段
    vector<size_t> accessPattern = {0, 5, 2, 8, 1, 9, 3, 7, 4, 6};

    auto start = chrono::high_resolution_clock::now();

    volatile size_t boostSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        for (size_t index : accessPattern) {
            auto result = accessBoost(index);
            boostSum += result.length();
        }
    }

    auto boostEnd = chrono::high_resolution_clock::now();

    volatile size_t fastSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        for (size_t index : accessPattern) {
            auto result = accessFast(index);
            fastSum += result.length();
        }
    }

    auto fastEnd = chrono::high_resolution_clock::now();

    auto boostTime = chrono::duration_cast<chrono::microseconds>(boostEnd - start);
    auto fastTime = chrono::duration_cast<chrono::microseconds>(fastEnd - boostEnd);

    double speedup = static_cast<double>(boostTime.count()) / fastTime.count();

    cout << "单字段访问 (" << benchmarkIterations << " 次迭代, " << accessPattern.size() << " 字段/次):\n";
    cout << "  Boost::split:    " << boostTime.count() << " μs\n";
    cout << "  FastFieldParser: " << fastTime.count() << " μs\n";
    cout << "  加速比:          " << fixed << setprecision(2) << speedup << "x\n";
    cout << "  校验和:          " << boostSum << " vs " << fastSum << "\n";

    APSARA_TEST_EQUAL(boostSum, fastSum);
}

void FastFieldParserBenchmark::TestProcessCredParsing() {
    cout << "\n=== 进程凭据解析性能测试 ===\n";

    vector<string> processStatusLines = {"Name:\tbash", "Uid:\t1000\t1000\t1000\t1000", "Gid:\t1000\t1000\t1000\t1000"};

    // Boost版本
    auto parseCredBoost = [](const vector<string>& lines) -> tuple<string, uint64_t, uint64_t> {
        string name;
        uint64_t uid = 0, gid = 0;

        for (const auto& line : lines) {
            vector<string> metric;
            boost::algorithm::split(
                metric, line, boost::algorithm::is_any_of("\t"), boost::algorithm::token_compress_on);

            if (metric.size() >= 2 && metric[0] == "Name:") {
                name = metric[1];
            } else if (metric.size() >= 3 && metric[0] == "Uid:") {
                uid = static_cast<uint64_t>(stoull(metric[1]));
            } else if (metric.size() >= 3 && metric[0] == "Gid:") {
                gid = static_cast<uint64_t>(stoull(metric[1]));
            }
        }

        return {name, uid, gid};
    };

    // Fast版本
    auto parseCredFast = [](const vector<string>& lines) -> tuple<string, uint64_t, uint64_t> {
        string name;
        uint64_t uid = 0, gid = 0;

        for (const auto& line : lines) {
            FastFieldParser parser(line, '\t');

            auto firstField = parser.GetField(0);
            if (firstField == "Name:") {
                auto nameField = parser.GetField(1);
                name = string(nameField);
            } else if (firstField == "Uid:") {
                uid = parser.GetFieldAs<uint64_t>(1, 0);
            } else if (firstField == "Gid:") {
                gid = parser.GetFieldAs<uint64_t>(1, 0);
            }
        }

        return {name, uid, gid};
    };

    // 验证正确性
    {
        auto boostResult = parseCredBoost(processStatusLines);
        auto fastResult = parseCredFast(processStatusLines);

        APSARA_TEST_EQUAL(get<0>(boostResult), get<0>(fastResult));
        APSARA_TEST_EQUAL(get<1>(boostResult), get<1>(fastResult));
        APSARA_TEST_EQUAL(get<2>(boostResult), get<2>(fastResult));
        cout << "✅ 结果正确性验证通过\n";
    }

    // 性能测试
    auto start = chrono::high_resolution_clock::now();

    volatile size_t boostSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        auto result = parseCredBoost(processStatusLines);
        boostSum += get<0>(result).length() + get<1>(result) + get<2>(result);
    }

    auto boostEnd = chrono::high_resolution_clock::now();

    volatile size_t fastSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        auto result = parseCredFast(processStatusLines);
        fastSum += get<0>(result).length() + get<1>(result) + get<2>(result);
    }

    auto fastEnd = chrono::high_resolution_clock::now();

    auto boostTime = chrono::duration_cast<chrono::microseconds>(boostEnd - start);
    auto fastTime = chrono::duration_cast<chrono::microseconds>(fastEnd - boostEnd);

    double speedup = static_cast<double>(boostTime.count()) / fastTime.count();

    cout << "进程凭据解析 (" << benchmarkIterations << " 次迭代):\n";
    cout << "  Boost::split:    " << boostTime.count() << " μs\n";
    cout << "  FastFieldParser: " << fastTime.count() << " μs\n";
    cout << "  加速比:          " << fixed << setprecision(2) << speedup << "x\n";
    cout << "  校验和:          " << boostSum << " vs " << fastSum << "\n";

    APSARA_TEST_EQUAL(boostSum, fastSum);
}

void FastFieldParserBenchmark::TestUptimeParsing() {
    cout << "\n=== 系统Uptime解析性能测试 ===\n";

    string uptimeLine = "183857.30 1969716.84";

    // Boost版本
    auto parseUptimeBoost = [](const string& line) -> double {
        vector<string> uptimeMetric;
        boost::split(uptimeMetric, line, boost::is_any_of(" "), boost::token_compress_on);
        return uptimeMetric.empty() ? 0.0 : stod(uptimeMetric[0]);
    };

    // Fast版本
    auto parseUptimeFast = [](const string& line) -> double { return FastParse::GetFieldAs<double>(line, 0, 0.0); };

    // 验证正确性
    {
        auto boostResult = parseUptimeBoost(uptimeLine);
        auto fastResult = parseUptimeFast(uptimeLine);

        APSARA_TEST_EQUAL(boostResult, fastResult);
        cout << "✅ 结果正确性验证通过 (uptime: " << boostResult << ")\n";
    }

    // 性能测试
    auto start = chrono::high_resolution_clock::now();

    volatile double boostSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        boostSum += parseUptimeBoost(uptimeLine);
    }

    auto boostEnd = chrono::high_resolution_clock::now();

    volatile double fastSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        fastSum += parseUptimeFast(uptimeLine);
    }

    auto fastEnd = chrono::high_resolution_clock::now();

    auto boostTime = chrono::duration_cast<chrono::microseconds>(boostEnd - start);
    auto fastTime = chrono::duration_cast<chrono::microseconds>(fastEnd - boostEnd);

    double speedup = static_cast<double>(boostTime.count()) / fastTime.count();

    cout << "Uptime解析 (" << benchmarkIterations << " 次迭代):\n";
    cout << "  Boost::split:    " << boostTime.count() << " μs\n";
    cout << "  FastFieldParser: " << fastTime.count() << " μs\n";
    cout << "  加速比:          " << fixed << setprecision(2) << speedup << "x\n";

    APSARA_TEST_EQUAL(static_cast<int>(boostSum), static_cast<int>(fastSum));
}

void FastFieldParserBenchmark::TestBatchFieldParsing() {
    cout << "\n=== 批量字段解析性能测试 ===\n";

    const string testLine = "123 456 789 101112 131415 161718 192021 222324 252627 282930 313233 343536 373839 404142";
    const int numFields = 14; // 模拟磁盘状态的14个字段

    // Fast版本 - 逐个调用（原方式）
    auto parseIndividual = [&]() -> vector<uint64_t> {
        vector<uint64_t> result;
        FastFieldParser parser(testLine);
        for (int i = 0; i < numFields; ++i) {
            result.push_back(parser.GetFieldAs<uint64_t>(i, 0));
        }
        return result;
    };

    // Fast版本 - 批量调用（新优化）
    auto parseBatch = [&]() -> vector<uint64_t> {
        FastFieldParser parser(testLine);
        return parser.GetFieldsAs<uint64_t>(0, numFields, 0);
    };

    // 验证正确性
    {
        auto individualResult = parseIndividual();
        auto batchResult = parseBatch();

        APSARA_TEST_EQUAL(individualResult.size(), batchResult.size());
        for (size_t i = 0; i < individualResult.size(); ++i) {
            APSARA_TEST_EQUAL(individualResult[i], batchResult[i]);
        }
        cout << "✅ 结果正确性验证通过 (磁盘状态14字段解析)\n";
    }

    // 性能测试
    auto start = chrono::high_resolution_clock::now();

    volatile uint64_t individualSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        auto result = parseIndividual();
        for (uint64_t val : result) {
            individualSum += val;
        }
    }

    auto individualEnd = chrono::high_resolution_clock::now();

    volatile uint64_t batchSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        auto result = parseBatch();
        for (uint64_t val : result) {
            batchSum += val;
        }
    }

    auto batchEnd = chrono::high_resolution_clock::now();

    auto individualTime = chrono::duration_cast<chrono::microseconds>(individualEnd - start);
    auto batchTime = chrono::duration_cast<chrono::microseconds>(batchEnd - individualEnd);

    double speedup = static_cast<double>(individualTime.count()) / batchTime.count();

    cout << "批量字段解析 (" << benchmarkIterations << " 次迭代, " << numFields << " 字段):\n";
    cout << "  逐个调用:        " << individualTime.count() << " μs (14次遍历)\n";
    cout << "  批量调用:        " << batchTime.count() << " μs (1次遍历)\n";
    cout << "  加速比:          " << fixed << setprecision(2) << speedup << "x\n";
    cout << "  校验和:          " << individualSum << " vs " << batchSum << "\n";
    cout << "  遍历减少:        " << fixed << setprecision(1) << (14.0 / 1.0) << "x (从14次到1次)\n";

    APSARA_TEST_EQUAL(individualSum, batchSum);
}

void FastFieldParserBenchmark::TestProcessMemoryParsing() {
    cout << "\n=== 进程内存解析性能测试 ===\n";

    const string memLine = "12345 6789 1011 1213 1415 1617 1819";
    const uint64_t PAGE_SIZE = 4096;

    // Boost版本
    auto parseMemBoost = [&]() -> tuple<uint64_t, uint64_t, uint64_t> {
        vector<string> processMemoryMetric;
        boost::algorithm::split(processMemoryMetric, memLine, boost::is_any_of(" "), boost::token_compress_on);

        if (processMemoryMetric.size() >= 3) {
            uint64_t size = stoull(processMemoryMetric[0]) * PAGE_SIZE;
            uint64_t resident = stoull(processMemoryMetric[1]) * PAGE_SIZE;
            uint64_t share = stoull(processMemoryMetric[2]) * PAGE_SIZE;
            return {size, resident, share};
        }
        return {0, 0, 0};
    };

    // Fast版本 - 批量解析
    auto parseMemFast = [&]() -> tuple<uint64_t, uint64_t, uint64_t> {
        auto memValues = FastParse::GetFieldsAs<uint64_t>(memLine, 0, 3, 0);

        if (memValues.size() >= 3) {
            uint64_t size = memValues[0] * PAGE_SIZE;
            uint64_t resident = memValues[1] * PAGE_SIZE;
            uint64_t share = memValues[2] * PAGE_SIZE;
            return {size, resident, share};
        }
        return {0, 0, 0};
    };

    // 验证正确性
    {
        auto boostResult = parseMemBoost();
        auto fastResult = parseMemFast();

        APSARA_TEST_EQUAL(get<0>(boostResult), get<0>(fastResult));
        APSARA_TEST_EQUAL(get<1>(boostResult), get<1>(fastResult));
        APSARA_TEST_EQUAL(get<2>(boostResult), get<2>(fastResult));
        cout << "✅ 结果正确性验证通过\n";
    }

    // 性能测试
    auto start = chrono::high_resolution_clock::now();

    volatile uint64_t boostSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        auto result = parseMemBoost();
        boostSum += get<0>(result) + get<1>(result) + get<2>(result);
    }

    auto boostEnd = chrono::high_resolution_clock::now();

    volatile uint64_t fastSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        auto result = parseMemFast();
        fastSum += get<0>(result) + get<1>(result) + get<2>(result);
    }

    auto fastEnd = chrono::high_resolution_clock::now();

    auto boostTime = chrono::duration_cast<chrono::microseconds>(boostEnd - start);
    auto fastTime = chrono::duration_cast<chrono::microseconds>(fastEnd - boostEnd);

    double speedup = static_cast<double>(boostTime.count()) / fastTime.count();

    cout << "进程内存解析 (" << benchmarkIterations << " 次迭代):\n";
    cout << "  Boost::split:    " << boostTime.count() << " μs\n";
    cout << "  FastFieldParser: " << fastTime.count() << " μs\n";
    cout << "  加速比:          " << fixed << setprecision(2) << speedup << "x\n";
    cout << "  校验和:          " << boostSum << " vs " << fastSum << "\n";

    APSARA_TEST_EQUAL(boostSum, fastSum);
}

void FastFieldParserBenchmark::TestProcessArgsParsing() {
    cout << "\n=== 进程参数解析性能测试 ===\n";

    const string cmdLine = "/usr/bin/python3 -u /opt/script.py --config=/etc/app.conf --verbose --timeout=30";

    // Boost版本
    auto parseArgsBoost = [&]() -> vector<string> {
        vector<string> cmdlineMetric;
        boost::algorithm::split(cmdlineMetric, cmdLine, boost::is_any_of(" "), boost::token_compress_on);
        vector<string> args;
        for (const auto& metric : cmdlineMetric) {
            args.push_back(metric);
        }
        return args;
    };

    // Fast版本
    auto parseArgsFast = [&]() -> vector<string> {
        FastFieldParser parser(cmdLine);
        size_t fieldCount = parser.GetFieldCount();

        vector<string> args;
        args.reserve(fieldCount);
        for (size_t i = 0; i < fieldCount; ++i) {
            auto field = parser.GetField(i);
            if (!field.empty()) {
                args.emplace_back(field);
            }
        }
        return args;
    };

    // 验证正确性
    {
        auto boostResult = parseArgsBoost();
        auto fastResult = parseArgsFast();

        APSARA_TEST_EQUAL(boostResult.size(), fastResult.size());
        for (size_t i = 0; i < boostResult.size(); ++i) {
            APSARA_TEST_EQUAL(boostResult[i], fastResult[i]);
        }
        cout << "✅ 结果正确性验证通过 (参数数量: " << boostResult.size() << ")\n";
    }

    // 性能测试
    auto start = chrono::high_resolution_clock::now();

    volatile size_t boostSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        auto result = parseArgsBoost();
        for (const auto& arg : result) {
            boostSum += arg.length();
        }
    }

    auto boostEnd = chrono::high_resolution_clock::now();

    volatile size_t fastSum = 0;
    for (int iter = 0; iter < benchmarkIterations; ++iter) {
        auto result = parseArgsFast();
        for (const auto& arg : result) {
            fastSum += arg.length();
        }
    }

    auto fastEnd = chrono::high_resolution_clock::now();

    auto boostTime = chrono::duration_cast<chrono::microseconds>(boostEnd - start);
    auto fastTime = chrono::duration_cast<chrono::microseconds>(fastEnd - boostEnd);

    double speedup = static_cast<double>(boostTime.count()) / fastTime.count();

    cout << "进程参数解析 (" << benchmarkIterations << " 次迭代):\n";
    cout << "  Boost::split:    " << boostTime.count() << " μs\n";
    cout << "  FastFieldParser: " << fastTime.count() << " μs\n";
    cout << "  加速比:          " << fixed << setprecision(2) << speedup << "x\n";
    cout << "  校验和:          " << boostSum << " vs " << fastSum << "\n";

    APSARA_TEST_EQUAL(boostSum, fastSum);
}

// 注册测试用例
TEST_F(FastFieldParserBenchmark, CPUStatParsing) {
    TestCPUStatParsing();
}

TEST_F(FastFieldParserBenchmark, SocketStatParsing) {
    TestSocketStatParsing();
}

TEST_F(FastFieldParserBenchmark, LoadStatParsing) {
    TestLoadStatParsing();
}

TEST_F(FastFieldParserBenchmark, NetDevParsing) {
    TestNetDevParsing();
}

TEST_F(FastFieldParserBenchmark, FieldAccess) {
    TestFieldAccess();
}

TEST_F(FastFieldParserBenchmark, ProcessCredParsing) {
    TestProcessCredParsing();
}

TEST_F(FastFieldParserBenchmark, UptimeParsing) {
    TestUptimeParsing();
}

TEST_F(FastFieldParserBenchmark, BatchFieldParsing) {
    TestBatchFieldParsing();
}

TEST_F(FastFieldParserBenchmark, ProcessMemoryParsing) {
    TestProcessMemoryParsing();
}

TEST_F(FastFieldParserBenchmark, ProcessArgsParsing) {
    TestProcessArgsParsing();
}

} // namespace logtail

UNIT_TEST_MAIN
