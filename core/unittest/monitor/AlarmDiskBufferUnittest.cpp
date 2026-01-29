// Copyright 2026 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <filesystem>
#include <sstream>
#include <vector>

#include "rapidjson/document.h"

#include "app_config/AppConfig.h"
#include "application/Application.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/FileSystemUtil.h"
#include "common/Lock.h"
#include "common/StringTools.h"
#include "models/LogEvent.h"
#include "monitor/AlarmManager.h"
#include "monitor/SelfMonitorServer.h"
#include "unittest/Unittest.h"

namespace logtail {

class AlarmDiskBufferUnittest : public ::testing::Test {
public:
    void SetUp() override {
        // 清理所有状态（包括全局静态变量）
        AlarmManager::GetInstance()->ClearTestState();
        AlarmManager::GetInstance()->mAllAlarmMap.clear();
        SelfMonitorServer::GetInstance()->RemoveAlarmPipeline();

        // 设置测试目录
        mTestDir = "/tmp/alarm_disk_buffer_test_" + ToString(time(nullptr));
        Mkdirs(mTestDir);

        // Mock Application::GetStartTime - 每次测试使用不同的时间，确保文件路径不同
        Application::GetInstance()->mStartTime = time(nullptr) + rand() % 1000000;

        // 设置测试用的 run dir
        mOriginalRunDir = GetAgentRunDir();
    }

    void TearDown() override {
        // 清理文件
        if (std::filesystem::exists(mTestDir)) {
            std::error_code ec;
            std::filesystem::remove_all(std::filesystem::path(mTestDir), ec);
        }

        // 清理所有状态（包括全局静态变量和文件）
        AlarmManager::GetInstance()->ClearTestState();
        AlarmManager::GetInstance()->mAllAlarmMap.clear();
        SelfMonitorServer::GetInstance()->RemoveAlarmPipeline();
    }

    std::string GetAlarmFileContent() {
        if (AlarmManager::GetInstance()->mAlarmDiskBufferFilePath.empty()) {
            return "";
        }
        const std::string& filePath = AlarmManager::GetInstance()->mAlarmDiskBufferFilePath;
        if (!CheckExistance(filePath)) {
            return "";
        }
        std::string content;
        FileReadResult result = ReadFileContent(filePath, content, 1024 * 1024);
        return (result == FileReadResult::kOK) ? content : "";
    }

    int32_t CountAlarmsInFile() {
        std::string content = GetAlarmFileContent();
        if (content.empty()) {
            return 0;
        }
        int32_t count = 0;
        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty()) {
                rapidjson::Document doc;
                if (!doc.Parse(line.data(), line.size()).HasParseError() && doc.IsObject()) {
                    count++;
                }
            }
        }
        return count;
    }

    int32_t GetAlarmCountFromFile(const std::string& region,
                                  const std::string& alarmType,
                                  const std::string& projectName,
                                  const std::string& category,
                                  const std::string& config,
                                  const std::string& level) {
        std::string content = GetAlarmFileContent();
        if (content.empty()) {
            return 0;
        }
        std::istringstream iss(content);
        std::string line;
        int32_t lastCount = 0;
        while (std::getline(iss, line)) {
            if (!line.empty()) {
                rapidjson::Document doc;
                if (!doc.Parse(line.data(), line.size()).HasParseError() && doc.IsObject()) {
                    if (doc.HasMember("region") && doc["region"].IsString() && doc["region"].GetString() == region
                        && doc.HasMember("alarm_type") && doc["alarm_type"].IsString()
                        && doc["alarm_type"].GetString() == alarmType && doc.HasMember("alarm_level")
                        && doc["alarm_level"].IsString() && doc["alarm_level"].GetString() == level) {
                        // 检查 project_name, category, config 是否匹配
                        bool match = true;
                        if (!projectName.empty()) {
                            const auto& pn = doc.FindMember("project_name");
                            if (pn == doc.MemberEnd() || !pn->value.IsString()
                                || pn->value.GetString() != projectName) {
                                match = false;
                            }
                        }
                        if (match && !category.empty()) {
                            const auto& cat = doc.FindMember("category");
                            if (cat == doc.MemberEnd() || !cat->value.IsString()
                                || cat->value.GetString() != category) {
                                match = false;
                            }
                        }
                        if (match && !config.empty()) {
                            const auto& cfg = doc.FindMember("config");
                            if (cfg == doc.MemberEnd() || !cfg->value.IsString() || cfg->value.GetString() != config) {
                                match = false;
                            }
                        }
                        if (match && doc.HasMember("alarm_count") && doc["alarm_count"].IsString()) {
                            try {
                                lastCount = std::stoi(doc["alarm_count"].GetString());
                            } catch (...) {
                                // 继续处理下一条
                            }
                        }
                    }
                }
            }
        }
        return lastCount;
    }

    int32_t CountAlarmsInBuffer(const std::string& region) {
        PTScopedLock lock(AlarmManager::GetInstance()->mAlarmBufferMutex);
        auto it = AlarmManager::GetInstance()->mAllAlarmMap.find(region);
        if (it == AlarmManager::GetInstance()->mAllAlarmMap.end()) {
            return 0;
        }
        int32_t total = 0;
        const auto& alarmVec = *it->second.first;
        for (const auto& alarmMap : alarmVec) {
            for (const auto& [key, msg] : alarmMap) {
                total += msg->mCount;
            }
        }
        return total;
    }

    int32_t CountAlarmsInPipelineEventGroups(const std::vector<PipelineEventGroup>& groups) {
        int32_t total = 0;
        for (const auto& group : groups) {
            for (const auto& evt : group.GetEvents()) {
                if (evt.Is<LogEvent>()) {
                    total++;
                }
            }
        }
        return total;
    }

    std::string mTestDir;
    std::string mOriginalRunDir;

    // 测试方法声明
    void TestWriteAlarmToFileWhenPipelineNotReady();
    void TestAlarmDeduplicationInFile();
    void TestReadAndSendAlarmsFromFile();
    void TestFileSendRetry();
    void TestStopWritingAfter60s();
    void TestNoDataLossOrDuplication();
};

APSARA_UNIT_TEST_CASE(AlarmDiskBufferUnittest, TestWriteAlarmToFileWhenPipelineNotReady, 0);
APSARA_UNIT_TEST_CASE(AlarmDiskBufferUnittest, TestAlarmDeduplicationInFile, 1);
APSARA_UNIT_TEST_CASE(AlarmDiskBufferUnittest, TestReadAndSendAlarmsFromFile, 2);
APSARA_UNIT_TEST_CASE(AlarmDiskBufferUnittest, TestFileSendRetry, 3);
APSARA_UNIT_TEST_CASE(AlarmDiskBufferUnittest, TestStopWritingAfter60s, 4);
APSARA_UNIT_TEST_CASE(AlarmDiskBufferUnittest, TestNoDataLossOrDuplication, 5);

void AlarmDiskBufferUnittest::TestWriteAlarmToFileWhenPipelineNotReady() {
    // Pipeline 未就绪时，alarm 应该写入文件
    AlarmManager::GetInstance()->mAlarmPipelineReady.store(false);
    AlarmManager::GetInstance()->mStopWritingFile.store(false);

    AlarmManager::GetInstance()->SendAlarmWarning(
        USER_CONFIG_ALARM, "Test message", "Region1", "Project1", "Config1", "Cat1");

    // 检查文件是否存在
    APSARA_TEST_TRUE(!AlarmManager::GetInstance()->mAlarmDiskBufferFilePath.empty());
    APSARA_TEST_TRUE(CheckExistance(AlarmManager::GetInstance()->mAlarmDiskBufferFilePath));

    // 检查文件内容
    int32_t fileAlarmCount = CountAlarmsInFile();
    APSARA_TEST_EQUAL(1, fileAlarmCount);

    // 检查 buffer 应该为空（pipeline 未就绪时不写 buffer）
    int32_t bufferCount = CountAlarmsInBuffer("Region1");
    APSARA_TEST_EQUAL(0, bufferCount);
}

void AlarmDiskBufferUnittest::TestAlarmDeduplicationInFile() {
    // 测试文件中的去重统计
    AlarmManager::GetInstance()->mAlarmPipelineReady.store(false);
    AlarmManager::GetInstance()->mStopWritingFile.store(false);

    // 发送相同的 alarm 3 次
    AlarmManager::GetInstance()->SendAlarmWarning(
        USER_CONFIG_ALARM, "Test message", "Region1", "Project1", "Config1", "Cat1");
    AlarmManager::GetInstance()->SendAlarmWarning(
        USER_CONFIG_ALARM, "Test message", "Region1", "Project1", "Config1", "Cat1");
    AlarmManager::GetInstance()->SendAlarmWarning(
        USER_CONFIG_ALARM, "Test message", "Region1", "Project1", "Config1", "Cat1");

    // 关闭文件句柄，确保数据已刷新
    AlarmManager::GetInstance()->CloseAlarmDiskBufferFile();

    // 文件应该有 3 条记录（写入时每条都是 count=1）
    int32_t fileAlarmCount = CountAlarmsInFile();
    APSARA_TEST_EQUAL(3, fileAlarmCount);

    // 文件中的每条记录 count 都是 1（写入时固定为 1）
    int32_t count = GetAlarmCountFromFile("Region1", "USER_CONFIG_ALARM", "Project1", "Cat1", "Config1", "1");
    APSARA_TEST_EQUAL(1, count);

    // 读取文件时，应该累加 count 为 3
    std::vector<PipelineEventGroup> fileGroups;
    std::map<std::string, std::string> regionToRawJson;
    bool hasData = AlarmManager::GetInstance()->ReadAlarmsFromFile(fileGroups, regionToRawJson);
    APSARA_TEST_TRUE(hasData);
    APSARA_TEST_EQUAL(1U, fileGroups.size());

    int32_t totalEvents = CountAlarmsInPipelineEventGroups(fileGroups);
    APSARA_TEST_EQUAL(1, totalEvents); // 去重后只有 1 个事件

    // 验证累加后的 count 是 3
    bool foundCount3 = false;
    for (const auto& group : fileGroups) {
        for (const auto& evt : group.GetEvents()) {
            if (evt.Is<LogEvent>()) {
                const auto& logEvt = evt.Cast<LogEvent>();
                std::string countStr = logEvt.GetContent("alarm_count").to_string();
                if (countStr == "3") {
                    foundCount3 = true;
                }
            }
        }
    }
    APSARA_TEST_TRUE(foundCount3);
}

void AlarmDiskBufferUnittest::TestReadAndSendAlarmsFromFile() {
    // 先写入一些 alarm 到文件
    AlarmManager::GetInstance()->mAlarmPipelineReady.store(false);
    AlarmManager::GetInstance()->mStopWritingFile.store(false);

    AlarmManager::GetInstance()->SendAlarmWarning(USER_CONFIG_ALARM, "Msg1", "Region1", "Project1", "Config1", "Cat1");
    AlarmManager::GetInstance()->SendAlarmWarning(
        GLOBAL_CONFIG_ALARM, "Msg2", "Region2", "Project2", "Config2", "Cat2");

    // 关闭文件句柄，确保数据已刷新
    AlarmManager::GetInstance()->CloseAlarmDiskBufferFile();

    int32_t fileAlarmCount = CountAlarmsInFile();
    APSARA_TEST_EQUAL(2, fileAlarmCount);

    // 设置 pipeline 就绪并读取文件
    CollectionPipelineContext ctx;
    ctx.SetProcessQueueKey(123);

    // 由于无法直接 mock ProcessorRunner，我们需要通过其他方式验证
    // 这里先测试 ReadAlarmsFromFile 的功能
    std::vector<PipelineEventGroup> fileGroups;
    std::map<std::string, std::string> regionToRawJson;
    bool hasData = AlarmManager::GetInstance()->ReadAlarmsFromFile(fileGroups, regionToRawJson);
    APSARA_TEST_TRUE(hasData);
    APSARA_TEST_EQUAL(2U, fileGroups.size());

    int32_t totalEvents = CountAlarmsInPipelineEventGroups(fileGroups);
    APSARA_TEST_EQUAL(2, totalEvents);

    // 验证文件内容正确
    bool foundRegion1 = false;
    bool foundRegion2 = false;
    for (const auto& group : fileGroups) {
        std::string region = group.GetMetadata(EventGroupMetaKey::INTERNAL_DATA_TARGET_REGION).to_string();
        if (region == "Region1") {
            foundRegion1 = true;
            APSARA_TEST_EQUAL(1U, group.GetEvents().size());
        } else if (region == "Region2") {
            foundRegion2 = true;
            APSARA_TEST_EQUAL(1U, group.GetEvents().size());
        }
    }
    APSARA_TEST_TRUE(foundRegion1);
    APSARA_TEST_TRUE(foundRegion2);
}

void AlarmDiskBufferUnittest::TestFileSendRetry() {
    // 测试文件发送失败后的重试逻辑
    AlarmManager::GetInstance()->mAlarmPipelineReady.store(false);
    AlarmManager::GetInstance()->mStopWritingFile.store(false);

    AlarmManager::GetInstance()->SendAlarmWarning(
        USER_CONFIG_ALARM, "Test message", "Region1", "Project1", "Config1", "Cat1");

    // 设置 pipeline
    CollectionPipelineContext ctx;
    ctx.SetProcessQueueKey(123);
    SelfMonitorServer::GetInstance()->UpdateAlarmPipeline(&ctx, 0);

    // 第一次 SendAlarms 应该读取文件
    // 由于无法 mock PushQueue，我们只能验证 ReadAlarmsFromFile 的功能
    std::vector<PipelineEventGroup> fileGroups;
    std::map<std::string, std::string> regionToRawJson;
    bool hasData = AlarmManager::GetInstance()->ReadAlarmsFromFile(fileGroups, regionToRawJson);
    APSARA_TEST_TRUE(hasData);

    // 模拟发送失败：不调用 DeleteAlarmFile
    // 文件应该仍然存在
    APSARA_TEST_TRUE(CheckExistance(AlarmManager::GetInstance()->mAlarmDiskBufferFilePath));
}

void AlarmDiskBufferUnittest::TestStopWritingAfter60s() {
    // 测试超过 60s 后停止写文件
    AlarmManager::GetInstance()->mAlarmPipelineReady.store(false);
    AlarmManager::GetInstance()->mStopWritingFile.store(false);

    // 设置启动时间为 61 秒前
    Application::GetInstance()->mStartTime = time(nullptr) - 61;

    // 尝试发送 alarm
    AlarmManager::GetInstance()->SendAlarmWarning(
        USER_CONFIG_ALARM, "Test message", "Region1", "Project1", "Config1", "Cat1");

    // 应该已停止写文件
    APSARA_TEST_TRUE(AlarmManager::GetInstance()->mStopWritingFile.load());

    // 文件应该不存在（因为超过 60s 且 pipeline 未就绪，alarm 被丢弃）
    // 注意：如果之前没有文件，mAlarmDiskBufferFilePath 可能为空
    if (!AlarmManager::GetInstance()->mAlarmDiskBufferFilePath.empty()) {
        APSARA_TEST_FALSE(CheckExistance(AlarmManager::GetInstance()->mAlarmDiskBufferFilePath));
    }

    // buffer 也应该为空（alarm 被丢弃）
    int32_t bufferCount = CountAlarmsInBuffer("Region1");
    APSARA_TEST_EQUAL(0, bufferCount);
}

void AlarmDiskBufferUnittest::TestNoDataLossOrDuplication() {
    // 综合测试：确保没有数据遗漏或重复
    AlarmManager::GetInstance()->mAllAlarmMap.clear();
    AlarmManager::GetInstance()->mAlarmPipelineReady.store(false);
    AlarmManager::GetInstance()->mStopWritingFile.store(false);
    Application::GetInstance()->mStartTime = time(nullptr);

    // 阶段1：Pipeline 未就绪，发送多个 alarm（包括重复的）
    AlarmManager::GetInstance()->SendAlarmWarning(USER_CONFIG_ALARM, "Msg1", "Region1", "Project1", "Config1", "Cat1");
    AlarmManager::GetInstance()->SendAlarmWarning(
        USER_CONFIG_ALARM, "Msg1", "Region1", "Project1", "Config1", "Cat1"); // 重复
    AlarmManager::GetInstance()->SendAlarmWarning(
        GLOBAL_CONFIG_ALARM, "Msg2", "Region1", "Project1", "Config1", "Cat1");
    AlarmManager::GetInstance()->SendAlarmWarning(USER_CONFIG_ALARM, "Msg3", "Region2", "Project2", "Config2", "Cat2");

    // 关闭文件句柄，确保数据已刷新
    AlarmManager::GetInstance()->CloseAlarmDiskBufferFile();

    // 验证文件中有 4 条记录（写入时每条都是 count=1）
    int32_t fileAlarmCount = CountAlarmsInFile();
    APSARA_TEST_EQUAL(4, fileAlarmCount);

    // 文件中的每条记录 count 都是 1（写入时固定为 1）
    int32_t count1 = GetAlarmCountFromFile("Region1", "USER_CONFIG_ALARM", "Project1", "Cat1", "Config1", "1");
    APSARA_TEST_EQUAL(1, count1);

    // 阶段2：Pipeline 就绪，读取文件并发送
    std::vector<PipelineEventGroup> fileGroups;
    std::map<std::string, std::string> regionToRawJson;
    bool hasData = AlarmManager::GetInstance()->ReadAlarmsFromFile(fileGroups, regionToRawJson);
    APSARA_TEST_TRUE(hasData);

    // 验证读取的数据正确
    // 读取时会按 key 去重累加：
    // - Region1 + USER_CONFIG_ALARM + Project1 + Cat1 + Config1 + level1: count=2 (2条相同告警累加)
    // - Region1 + GLOBAL_CONFIG_ALARM + Project1 + Cat1 + Config1 + level1: count=1
    // - Region2 + USER_CONFIG_ALARM + Project2 + Cat2 + Config2 + level1: count=1
    // 总共 3 个事件
    int32_t totalEvents = CountAlarmsInPipelineEventGroups(fileGroups);
    APSARA_TEST_EQUAL(3, totalEvents);

    // 验证 count 正确
    // Region1 的 USER_CONFIG_ALARM 应该只有 1 个事件，count=2（累加后）
    bool foundCount2 = false;
    int32_t region1UserConfigCount = 0;
    for (const auto& group : fileGroups) {
        std::string region = group.GetMetadata(EventGroupMetaKey::INTERNAL_DATA_TARGET_REGION).to_string();
        for (const auto& evt : group.GetEvents()) {
            if (evt.Is<LogEvent>()) {
                const auto& logEvt = evt.Cast<LogEvent>();
                std::string alarmType = logEvt.GetContent("alarm_type").to_string();
                std::string countStr = logEvt.GetContent("alarm_count").to_string();
                if (alarmType == "USER_CONFIG_ALARM" && region == "Region1") {
                    region1UserConfigCount++;
                    if (countStr == "2") {
                        foundCount2 = true;
                    }
                } else {
                    // 其他 alarm 的 count 都应该是 1
                    APSARA_TEST_EQUAL("1", countStr);
                }
            }
        }
    }
    // Region1 的 USER_CONFIG_ALARM 应该有 1 个事件（累加后），count=2
    APSARA_TEST_EQUAL(1, region1UserConfigCount);
    // 应该找到 count=2 的事件
    APSARA_TEST_TRUE(foundCount2);

    // 阶段3：删除文件后，发送新的 alarm 应该写 buffer
    AlarmManager::GetInstance()->DeleteAlarmFile();
    AlarmManager::GetInstance()->CheckAndSetAlarmPipelineReady();

    AlarmManager::GetInstance()->SendAlarmWarning(USER_CONFIG_ALARM, "Msg4", "Region1", "Project1", "Config1", "Cat1");

    // 应该写 buffer，不写文件
    APSARA_TEST_FALSE(CheckExistance(AlarmManager::GetInstance()->mAlarmDiskBufferFilePath));
    int32_t bufferCount = CountAlarmsInBuffer("Region1");
    APSARA_TEST_EQUAL(1, bufferCount);

    // 阶段4：验证 buffer 中的 alarm 可以正常 flush
    std::vector<PipelineEventGroup> bufferGroups;
    AlarmManager::GetInstance()->FlushAllRegionAlarm(bufferGroups);
    APSARA_TEST_EQUAL(1U, bufferGroups.size());
    APSARA_TEST_EQUAL(1U, bufferGroups[0].GetEvents().size());
}

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
