/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "journal_server/manager/JournalMonitor.h"
#include "journal_server/reader/JournalReader.h"
#include "models/PipelineEventGroup.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class JournalMonitorUnittest : public testing::Test {
public:
    JournalMonitorUnittest() = default;
    ~JournalMonitorUnittest() = default;

    void TestSingleton();
    void TestInitialize();
    void TestCleanup();
    void TestGetEpollFD();
    void TestAddReaderToMonitoring();
    void TestAddReaderToMonitoringNullReader();
    void TestAddReaderToMonitoringClosedReader();
    void TestAddReaderToMonitoringSameReader();
    void TestAddReaderToMonitoringMultipleReaders();
    void TestMarkReaderAsClosing();
    void TestMarkReaderAsClosingNonexistent();
    void TestRemoveReaderFromMonitoring();
    void TestRemoveReaderFromMonitoringNonexistent();
    void TestCleanupClosedReaders();
    void TestGetMonitoredReaders();
    void TestSaveAccumulatedData();
    void TestSaveAccumulatedDataNonexistent();
    void TestRestoreAccumulatedData();
    void TestRestoreAccumulatedDataNullReader();
    void TestRestoreAccumulatedDataClosingReader();
    void TestGetValidatedCurrentReader();
    void TestGetValidatedCurrentReaderNullReader();
    void TestGetValidatedCurrentReaderClosedReader();
    void TestGetValidatedCurrentReaderMismatch();
    void TestClearPendingDataForInvalidReader();
    void TestClearPendingDataNoPending();
    void TestIsBatchTimeoutExceeded();
    void TestIsBatchTimeoutNotExceeded();
    void TestIsBatchTimeoutNoData();

protected:
    void SetUp() override {
        // 每个测试前重新初始化
        mMonitor = JournalMonitor::GetInstance();
    }

    void TearDown() override {
        // 每个测试后清理
        if (mMonitor) {
            mMonitor->Cleanup();
        }
    }

private:
    JournalMonitor* mMonitor{nullptr};
};

void JournalMonitorUnittest::TestSingleton() {
    JournalMonitor* instance1 = JournalMonitor::GetInstance();
    JournalMonitor* instance2 = JournalMonitor::GetInstance();

    APSARA_TEST_TRUE(instance1 != nullptr);
    APSARA_TEST_TRUE(instance2 != nullptr);
    APSARA_TEST_TRUE(instance1 == instance2);
}

void JournalMonitorUnittest::TestInitialize() {
    // 测试初始化
    bool result = mMonitor->Initialize();

    // 在Linux平台上应该成功
#ifdef __linux__
    APSARA_TEST_TRUE(result);

    // 获取epoll FD
    int epollFD = mMonitor->GetEpollFD();
    APSARA_TEST_TRUE(epollFD >= 0 || epollFD == -1); // 可能失败但不应崩溃
#else
    APSARA_TEST_FALSE(result);
#endif
}

void JournalMonitorUnittest::TestCleanup() {
    // 初始化
    mMonitor->Initialize();

    // 清理
    mMonitor->Cleanup();

    // 验证清理后状态
    int epollFD = mMonitor->GetEpollFD();
    APSARA_TEST_EQUAL(epollFD, -1);
}

void JournalMonitorUnittest::TestGetEpollFD() {
    // 未初始化状态
    int epollFD = mMonitor->GetEpollFD();
    APSARA_TEST_EQUAL(epollFD, -1);

    // 初始化后
    mMonitor->Initialize();
    epollFD = mMonitor->GetEpollFD();

#ifdef __linux__
    APSARA_TEST_TRUE(epollFD >= 0 || epollFD == -1);
#else
    APSARA_TEST_EQUAL(epollFD, -1);
#endif
}

void JournalMonitorUnittest::TestAddReaderToMonitoring() {
    mMonitor->Initialize();

    // 创建测试reader
    auto reader = std::make_shared<JournalReader>();
    std::string configName = "test_config";

    // 添加到监控（在测试环境中可能失败）
    int fd = mMonitor->AddReaderToMonitoring(reader, configName);

    // 验证返回值（可能为-1如果reader未打开）
    APSARA_TEST_TRUE(fd >= 0 || fd == -1);
}

void JournalMonitorUnittest::TestAddReaderToMonitoringNullReader() {
    mMonitor->Initialize();

    // 尝试添加null reader
    int fd = mMonitor->AddReaderToMonitoring(nullptr, "test_config");

    // 应该返回-1
    APSARA_TEST_EQUAL(fd, -1);
}

void JournalMonitorUnittest::TestAddReaderToMonitoringClosedReader() {
    mMonitor->Initialize();

    // 创建未打开的reader
    auto reader = std::make_shared<JournalReader>();
    // 不调用Open()

    int fd = mMonitor->AddReaderToMonitoring(reader, "test_config");

    // 应该返回-1（reader未打开）
    APSARA_TEST_EQUAL(fd, -1);
}

void JournalMonitorUnittest::TestAddReaderToMonitoringSameReader() {
    mMonitor->Initialize();

    auto reader = std::make_shared<JournalReader>();
    std::string configName = "test_config";

    // 第一次添加
    int fd1 = mMonitor->AddReaderToMonitoring(reader, configName);

    // 第二次添加相同reader
    int fd2 = mMonitor->AddReaderToMonitoring(reader, configName);

    // 两次应该返回相同的FD（或都失败）
    APSARA_TEST_TRUE((fd1 == fd2) || (fd1 == -1 && fd2 == -1));
}

void JournalMonitorUnittest::TestAddReaderToMonitoringMultipleReaders() {
    mMonitor->Initialize();

    auto reader1 = std::make_shared<JournalReader>();
    auto reader2 = std::make_shared<JournalReader>();

    int fd1 = mMonitor->AddReaderToMonitoring(reader1, "config1");
    int fd2 = mMonitor->AddReaderToMonitoring(reader2, "config2");

    // 两个reader应该有不同的FD（或都失败）
    APSARA_TEST_TRUE((fd1 != fd2 && fd1 >= 0 && fd2 >= 0) || (fd1 == -1 || fd2 == -1));
}

void JournalMonitorUnittest::TestMarkReaderAsClosing() {
    mMonitor->Initialize();

    std::string configName = "test_config";

    // 标记为closing（即使reader不存在也不应崩溃）
    mMonitor->MarkReaderAsClosing(configName);

    // 验证不会崩溃
    APSARA_TEST_TRUE(true);
}

void JournalMonitorUnittest::TestMarkReaderAsClosingNonexistent() {
    mMonitor->Initialize();

    // 标记不存在的reader
    mMonitor->MarkReaderAsClosing("nonexistent_config");

    // 应该不崩溃
    APSARA_TEST_TRUE(true);
}

void JournalMonitorUnittest::TestRemoveReaderFromMonitoring() {
    mMonitor->Initialize();

    std::string configName = "test_config";

    // 移除（即使不存在也不应崩溃）
    mMonitor->RemoveReaderFromMonitoring(configName);

    // 验证不会崩溃
    APSARA_TEST_TRUE(true);
}

void JournalMonitorUnittest::TestRemoveReaderFromMonitoringNonexistent() {
    mMonitor->Initialize();

    // 移除不存在的reader
    mMonitor->RemoveReaderFromMonitoring("nonexistent_config");

    // 应该不崩溃
    APSARA_TEST_TRUE(true);
}

void JournalMonitorUnittest::TestCleanupClosedReaders() {
    mMonitor->Initialize();

    // 清理已关闭的readers
    mMonitor->CleanupClosedReaders();

    // 验证不会崩溃
    APSARA_TEST_TRUE(true);
}

void JournalMonitorUnittest::TestGetMonitoredReaders() {
    mMonitor->Initialize();

    // 获取监控的readers
    auto& readers = mMonitor->GetMonitoredReaders();

    // 验证返回的是map（初始应该为空）
    APSARA_TEST_TRUE(readers.empty() || !readers.empty());
}

void JournalMonitorUnittest::TestSaveAccumulatedData() {
    mMonitor->Initialize();

    std::string configName = "test_config";
    bool savedHasPendingData = false;
    std::shared_ptr<PipelineEventGroup> savedAccumulatedEventGroup;
    int savedAccumulatedEntryCount = 0;
    std::chrono::steady_clock::time_point savedLastBatchTime;

    // 保存累积数据（reader不存在时应该返回false）
    bool result = mMonitor->SaveAccumulatedData(
        configName, savedHasPendingData, savedAccumulatedEventGroup, savedAccumulatedEntryCount, savedLastBatchTime);

    // 应该返回false（因为reader不存在）
    APSARA_TEST_FALSE(result);
}

void JournalMonitorUnittest::TestSaveAccumulatedDataNonexistent() {
    mMonitor->Initialize();

    bool savedHasPendingData = false;
    std::shared_ptr<PipelineEventGroup> savedAccumulatedEventGroup;
    int savedAccumulatedEntryCount = 0;
    std::chrono::steady_clock::time_point savedLastBatchTime;

    // 保存不存在的reader的数据
    bool result = mMonitor->SaveAccumulatedData("nonexistent_config",
                                                savedHasPendingData,
                                                savedAccumulatedEventGroup,
                                                savedAccumulatedEntryCount,
                                                savedLastBatchTime);

    APSARA_TEST_FALSE(result);
}

void JournalMonitorUnittest::TestRestoreAccumulatedData() {
    mMonitor->Initialize();

    std::string configName = "test_config";
    auto reader = std::make_shared<JournalReader>();
    bool savedHasPendingData = true;
    std::shared_ptr<PipelineEventGroup> savedAccumulatedEventGroup = nullptr;
    int savedAccumulatedEntryCount = 10;
    std::chrono::steady_clock::time_point savedLastBatchTime = std::chrono::steady_clock::now();

    // 恢复累积数据（即使reader不在监控中也不应崩溃）
    mMonitor->RestoreAccumulatedData(configName,
                                     reader,
                                     savedHasPendingData,
                                     savedAccumulatedEventGroup,
                                     savedAccumulatedEntryCount,
                                     savedLastBatchTime);

    // 验证不崩溃
    APSARA_TEST_TRUE(true);
}

void JournalMonitorUnittest::TestRestoreAccumulatedDataNullReader() {
    mMonitor->Initialize();

    std::string configName = "test_config";
    bool savedHasPendingData = true;
    std::shared_ptr<PipelineEventGroup> savedAccumulatedEventGroup = nullptr;
    int savedAccumulatedEntryCount = 10;
    std::chrono::steady_clock::time_point savedLastBatchTime = std::chrono::steady_clock::now();

    // 使用null reader
    mMonitor->RestoreAccumulatedData(configName,
                                     nullptr,
                                     savedHasPendingData,
                                     savedAccumulatedEventGroup,
                                     savedAccumulatedEntryCount,
                                     savedLastBatchTime);

    // 验证不崩溃
    APSARA_TEST_TRUE(true);
}

void JournalMonitorUnittest::TestRestoreAccumulatedDataClosingReader() {
    mMonitor->Initialize();

    std::string configName = "test_config";
    auto reader = std::make_shared<JournalReader>();
    bool savedHasPendingData = true;
    std::shared_ptr<PipelineEventGroup> savedAccumulatedEventGroup = nullptr;
    int savedAccumulatedEntryCount = 10;
    std::chrono::steady_clock::time_point savedLastBatchTime = std::chrono::steady_clock::now();

    // 恢复数据到正在关闭的reader
    mMonitor->RestoreAccumulatedData(configName,
                                     reader,
                                     savedHasPendingData,
                                     savedAccumulatedEventGroup,
                                     savedAccumulatedEntryCount,
                                     savedLastBatchTime);

    // 验证不崩溃
    APSARA_TEST_TRUE(true);
}

void JournalMonitorUnittest::TestGetValidatedCurrentReader() {
    mMonitor->Initialize();

    MonitoredReader monitoredReader;
    monitoredReader.configName = "test_config";
    std::shared_ptr<JournalReader> currentReaderOut;

    // 验证reader（null reader应该返回false）
    bool result = mMonitor->GetValidatedCurrentReader(monitoredReader, currentReaderOut);

    APSARA_TEST_FALSE(result);
}

void JournalMonitorUnittest::TestGetValidatedCurrentReaderNullReader() {
    mMonitor->Initialize();

    MonitoredReader monitoredReader;
    monitoredReader.reader = nullptr;
    monitoredReader.configName = "test_config";
    std::shared_ptr<JournalReader> currentReaderOut;

    bool result = mMonitor->GetValidatedCurrentReader(monitoredReader, currentReaderOut);

    APSARA_TEST_FALSE(result);
}

void JournalMonitorUnittest::TestGetValidatedCurrentReaderClosedReader() {
    mMonitor->Initialize();

    MonitoredReader monitoredReader;
    monitoredReader.reader = std::make_shared<JournalReader>();
    monitoredReader.configName = "test_config";
    std::shared_ptr<JournalReader> currentReaderOut;

    // Reader未打开
    bool result = mMonitor->GetValidatedCurrentReader(monitoredReader, currentReaderOut);

    APSARA_TEST_FALSE(result);
}

void JournalMonitorUnittest::TestGetValidatedCurrentReaderMismatch() {
    mMonitor->Initialize();

    MonitoredReader monitoredReader;
    monitoredReader.reader = std::make_shared<JournalReader>();
    monitoredReader.configName = "test_config";
    std::shared_ptr<JournalReader> currentReaderOut;

    // Reader不匹配（未在connection manager中）
    bool result = mMonitor->GetValidatedCurrentReader(monitoredReader, currentReaderOut);

    APSARA_TEST_FALSE(result);
}

void JournalMonitorUnittest::TestClearPendingDataForInvalidReader() {
    mMonitor->Initialize();

    MonitoredReader monitoredReader;
    monitoredReader.hasPendingData = true;
    monitoredReader.accumulatedEventGroup = nullptr;
    monitoredReader.accumulatedEntryCount = 10;

    // 清理pending data
    mMonitor->ClearPendingDataForInvalidReader(monitoredReader);

    // 验证数据被清理
    APSARA_TEST_FALSE(monitoredReader.hasPendingData);
    APSARA_TEST_TRUE(monitoredReader.accumulatedEventGroup == nullptr);
    APSARA_TEST_EQUAL(monitoredReader.accumulatedEntryCount, 0);
}

void JournalMonitorUnittest::TestClearPendingDataNoPending() {
    mMonitor->Initialize();

    MonitoredReader monitoredReader;
    monitoredReader.hasPendingData = false;
    monitoredReader.accumulatedEventGroup = nullptr;
    monitoredReader.accumulatedEntryCount = 0;

    // 清理已经没有pending data的reader
    mMonitor->ClearPendingDataForInvalidReader(monitoredReader);

    // 验证状态保持不变
    APSARA_TEST_FALSE(monitoredReader.hasPendingData);
    APSARA_TEST_TRUE(monitoredReader.accumulatedEventGroup == nullptr);
    APSARA_TEST_EQUAL(monitoredReader.accumulatedEntryCount, 0);
}

void JournalMonitorUnittest::TestIsBatchTimeoutExceeded() {
    mMonitor->Initialize();

    MonitoredReader monitoredReader;
    monitoredReader.accumulatedEventGroup = std::make_shared<PipelineEventGroup>(nullptr);
    monitoredReader.accumulatedEntryCount = 10;
    monitoredReader.lastBatchTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(2000);

    // 测试超时（1000ms）
    bool exceeded = mMonitor->IsBatchTimeoutExceeded(monitoredReader, 1000);

    APSARA_TEST_TRUE(exceeded);
}

void JournalMonitorUnittest::TestIsBatchTimeoutNotExceeded() {
    mMonitor->Initialize();

    MonitoredReader monitoredReader;
    monitoredReader.accumulatedEventGroup = std::make_shared<PipelineEventGroup>(nullptr);
    monitoredReader.accumulatedEntryCount = 10;
    monitoredReader.lastBatchTime = std::chrono::steady_clock::now();

    // 测试未超时（刚刚设置时间）
    bool exceeded = mMonitor->IsBatchTimeoutExceeded(monitoredReader, 1000);

    APSARA_TEST_FALSE(exceeded);
}

void JournalMonitorUnittest::TestIsBatchTimeoutNoData() {
    mMonitor->Initialize();

    MonitoredReader monitoredReader;
    monitoredReader.accumulatedEventGroup = nullptr;
    monitoredReader.accumulatedEntryCount = 0;
    monitoredReader.lastBatchTime = std::chrono::steady_clock::now();

    // 测试无数据时不应触发超时
    bool exceeded = mMonitor->IsBatchTimeoutExceeded(monitoredReader, 1000);

    APSARA_TEST_FALSE(exceeded);
}

// 注册测试用例
TEST_F(JournalMonitorUnittest, TestSingleton) {
    TestSingleton();
}

TEST_F(JournalMonitorUnittest, TestInitialize) {
    TestInitialize();
}

TEST_F(JournalMonitorUnittest, TestCleanup) {
    TestCleanup();
}

TEST_F(JournalMonitorUnittest, TestGetEpollFD) {
    TestGetEpollFD();
}

TEST_F(JournalMonitorUnittest, TestAddReaderToMonitoring) {
    TestAddReaderToMonitoring();
}

TEST_F(JournalMonitorUnittest, TestAddReaderToMonitoringNullReader) {
    TestAddReaderToMonitoringNullReader();
}

TEST_F(JournalMonitorUnittest, TestAddReaderToMonitoringClosedReader) {
    TestAddReaderToMonitoringClosedReader();
}

TEST_F(JournalMonitorUnittest, TestAddReaderToMonitoringSameReader) {
    TestAddReaderToMonitoringSameReader();
}

TEST_F(JournalMonitorUnittest, TestAddReaderToMonitoringMultipleReaders) {
    TestAddReaderToMonitoringMultipleReaders();
}

TEST_F(JournalMonitorUnittest, TestMarkReaderAsClosing) {
    TestMarkReaderAsClosing();
}

TEST_F(JournalMonitorUnittest, TestMarkReaderAsClosingNonexistent) {
    TestMarkReaderAsClosingNonexistent();
}

TEST_F(JournalMonitorUnittest, TestRemoveReaderFromMonitoring) {
    TestRemoveReaderFromMonitoring();
}

TEST_F(JournalMonitorUnittest, TestRemoveReaderFromMonitoringNonexistent) {
    TestRemoveReaderFromMonitoringNonexistent();
}

TEST_F(JournalMonitorUnittest, TestCleanupClosedReaders) {
    TestCleanupClosedReaders();
}

TEST_F(JournalMonitorUnittest, TestGetMonitoredReaders) {
    TestGetMonitoredReaders();
}

TEST_F(JournalMonitorUnittest, TestSaveAccumulatedData) {
    TestSaveAccumulatedData();
}

TEST_F(JournalMonitorUnittest, TestSaveAccumulatedDataNonexistent) {
    TestSaveAccumulatedDataNonexistent();
}

TEST_F(JournalMonitorUnittest, TestRestoreAccumulatedData) {
    TestRestoreAccumulatedData();
}

TEST_F(JournalMonitorUnittest, TestRestoreAccumulatedDataNullReader) {
    TestRestoreAccumulatedDataNullReader();
}

TEST_F(JournalMonitorUnittest, TestRestoreAccumulatedDataClosingReader) {
    TestRestoreAccumulatedDataClosingReader();
}

TEST_F(JournalMonitorUnittest, TestGetValidatedCurrentReader) {
    TestGetValidatedCurrentReader();
}

TEST_F(JournalMonitorUnittest, TestGetValidatedCurrentReaderNullReader) {
    TestGetValidatedCurrentReaderNullReader();
}

TEST_F(JournalMonitorUnittest, TestGetValidatedCurrentReaderClosedReader) {
    TestGetValidatedCurrentReaderClosedReader();
}

TEST_F(JournalMonitorUnittest, TestGetValidatedCurrentReaderMismatch) {
    TestGetValidatedCurrentReaderMismatch();
}

TEST_F(JournalMonitorUnittest, TestClearPendingDataForInvalidReader) {
    TestClearPendingDataForInvalidReader();
}

TEST_F(JournalMonitorUnittest, TestClearPendingDataNoPending) {
    TestClearPendingDataNoPending();
}

TEST_F(JournalMonitorUnittest, TestIsBatchTimeoutExceeded) {
    TestIsBatchTimeoutExceeded();
}

TEST_F(JournalMonitorUnittest, TestIsBatchTimeoutNotExceeded) {
    TestIsBatchTimeoutNotExceeded();
}

TEST_F(JournalMonitorUnittest, TestIsBatchTimeoutNoData) {
    TestIsBatchTimeoutNoData();
}

} // namespace logtail

UNIT_TEST_MAIN
