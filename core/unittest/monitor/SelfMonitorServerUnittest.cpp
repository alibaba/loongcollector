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

#include <atomic>
#include <string>
#include <thread>

#include "models/PipelineEventGroup.h"
#include "monitor/AlarmManager.h"
#include "monitor/SelfMonitorServer.h"
#include "protobuf/models/ProtocolConversion.h"
#include "protobuf/models/pipeline_event_group.pb.h"
#include "unittest/Unittest.h"

namespace logtail {

class SelfMonitorServerUnittest : public ::testing::Test {
public:
    void SetUp() override {
        AlarmManager::GetInstance()->ForceToSend();
        FlushAlarms();
    }
    void TestReceiveGoAlarmsPB();
    void TestReceiveGoAlarmsPBInvalid();
    void TestReceiveGoAlarmsPBCrossBatch();
    void TestReceiveGoAlarmsPBDuplicateFields();
    void TestReceiveGoAlarmsPBMalformedBatch();
    void TestReceiveGoAlarmsPBOneof();
    void TestReceiveGoAlarmsPBBatchSizes();
    void TestReceiveGoAlarmsPBConcurrent();
    void TestReceiveGoAlarmsPBFinalMetadata();

private:
    static void AddContent(models::LogEvent* event, const std::string& key, const std::string& value) {
        auto* content = event->add_contents();
        content->set_key(key);
        content->set_value(value);
    }

    static std::vector<std::map<std::string, std::string>> FlushAlarms() {
        std::vector<PipelineEventGroup> groups;
        AlarmManager::GetInstance()->FlushAllRegionAlarm(groups);
        std::vector<std::map<std::string, std::string>> alarms;
        for (const auto& group : groups) {
            for (const auto& event : group.GetEvents()) {
                const auto& log = event.Cast<LogEvent>();
                std::map<std::string, std::string> fields;
                for (const char* key : {"alarm_type",
                                        "alarm_level",
                                        "alarm_message",
                                        "alarm_count",
                                        "project_name",
                                        "category",
                                        "config",
                                        "ip",
                                        "os",
                                        "ver"}) {
                    if (log.HasContent(key)) {
                        auto value = log.GetContent(key);
                        fields[key] = std::string(value.data(), value.size());
                    }
                }
                alarms.emplace_back(std::move(fields));
            }
        }
        return alarms;
    }

    // Serialize a PipelineEventGroup of alarm LogEvents into PB bytes, mirroring the
    // Go bridge payload produced by helper.TransferAlarmsToPipelineEventGroup.
    static std::string BuildAlarmsPB(const std::vector<std::map<std::string, std::string>>& alarms) {
        PipelineEventGroup group(std::make_shared<SourceBuffer>());
        for (const auto& alarm : alarms) {
            LogEvent* logEvent = group.AddLogEvent();
            logEvent->SetTimestamp(1700000000);
            for (const auto& kv : alarm) {
                logEvent->SetContent(kv.first, kv.second);
            }
        }
        models::PipelineEventGroup pb;
        std::string errMsg;
        if (!TransferPipelineEventGroupToPB(group, pb, errMsg)) {
            return "";
        }
        std::string bytes;
        bool serialized = pb.SerializeToString(&bytes);
        // Guard the boolean: on failure `bytes` may be empty/partial, which would make
        // downstream assertions misleading. Fail loudly with context and return "" so the
        // callers' APSARA_TEST_FALSE(bytes.empty()) check stops the test deterministically.
        EXPECT_TRUE(serialized) << "SerializeToString failed, ByteSizeLong=" << pb.ByteSizeLong();
        if (!serialized) {
            return "";
        }
        return bytes;
    }
};

// A batch with two well-formed alarms and one missing alarm_type: only the two valid
// alarms are forwarded to AlarmManager.
void SelfMonitorServerUnittest::TestReceiveGoAlarmsPB() {
    std::vector<std::map<std::string, std::string>> alarms = {
        {{"alarm_type", "PARSE_ERROR_ALARM"},
         {"alarm_level", "2"},
         {"alarm_message", "parse failed"},
         {"alarm_count", "3"},
         {"project_name", "proj-a"},
         {"category", "logstore-a"},
         {"config", "config-a/1"}},
        {{"alarm_type", "SEND_ALARM"}, {"alarm_level", "3"}, {"alarm_message", "send failed"}, {"alarm_count", "1"}},
        {{"alarm_level", "1"}, {"alarm_message", "no type, should be dropped"}},
    };
    std::string bytes = BuildAlarmsPB(alarms);
    APSARA_TEST_FALSE(bytes.empty());

    int32_t synced = SelfMonitorServer::GetInstance()->ReceiveGoAlarmsPB(bytes);
    APSARA_TEST_EQUAL(synced, 2);
}

// Empty and malformed payloads forward nothing.
void SelfMonitorServerUnittest::TestReceiveGoAlarmsPBInvalid() {
    APSARA_TEST_EQUAL(SelfMonitorServer::GetInstance()->ReceiveGoAlarmsPB(""), 0);
    APSARA_TEST_EQUAL(SelfMonitorServer::GetInstance()->ReceiveGoAlarmsPB("not-a-valid-protobuf-payload"), 0);
}

void SelfMonitorServerUnittest::TestReceiveGoAlarmsPBCrossBatch() {
    auto* server = SelfMonitorServer::GetInstance();
    auto first = BuildAlarmsPB({{{"alarm_type", "FIRST_BATCH"},
                                 {"alarm_message", "first 消息"},
                                 {"alarm_level", "3"},
                                 {"alarm_count", "7"},
                                 {"project_name", "project"},
                                 {"category", "store"},
                                 {"config", "config/2"}}});
    APSARA_TEST_EQUAL(server->ReceiveGoAlarmsPB(first), 1);
    first.assign(first.size(), 'x'); // AlarmManager must own retained fields after the input dies.
    APSARA_TEST_EQUAL(server->ReceiveGoAlarmsPB(BuildAlarmsPB({{{"alarm_message", "missing type"}}})), 0);
    APSARA_TEST_EQUAL(
        server->ReceiveGoAlarmsPB(BuildAlarmsPB({{{"alarm_type", "SECOND_BATCH"}, {"alarm_count", "invalid"}}})), 1);
    auto alarms = FlushAlarms();
    ASSERT_EQ(2U, alarms.size());
    std::map<std::string, std::map<std::string, std::string>> byType;
    for (auto& alarm : alarms) {
        auto type = alarm["alarm_type"];
        byType.emplace(type, std::move(alarm));
    }
    APSARA_TEST_EQUAL(byType["FIRST_BATCH"]["alarm_message"], "first 消息");
    APSARA_TEST_EQUAL(byType["FIRST_BATCH"]["config"], "config");
    APSARA_TEST_EQUAL(byType["FIRST_BATCH"]["alarm_count"], "7");
    APSARA_TEST_EQUAL(byType["SECOND_BATCH"]["alarm_message"], "");
    APSARA_TEST_EQUAL(byType["SECOND_BATCH"]["alarm_level"], "1");
    APSARA_TEST_EQUAL(byType["SECOND_BATCH"]["alarm_count"], "1");
    APSARA_TEST_TRUE(byType["SECOND_BATCH"].find("project_name") == byType["SECOND_BATCH"].end());
    APSARA_TEST_TRUE(byType["SECOND_BATCH"].find("category") == byType["SECOND_BATCH"].end());
    APSARA_TEST_TRUE(byType["SECOND_BATCH"].find("config") == byType["SECOND_BATCH"].end());
}

void SelfMonitorServerUnittest::TestReceiveGoAlarmsPBDuplicateFields() {
    models::PipelineEventGroup group;
    auto* event = group.mutable_logs()->add_events();
    AddContent(event, "alarm_count", "-1");
    AddContent(event, "alarm_message", "last field order is arbitrary");
    AddContent(event, "alarm_type", "OLD_TYPE");
    AddContent(event, "alarm_type", "LAST_TYPE");
    AddContent(event, "alarm_level", "1");
    AddContent(event, "alarm_level", "2");
    AddContent(event, "alarm_count", "3");
    AddContent(event, "alarm_time", "same length as alarm_type");
    AddContent(event, "alarm_times", "same length as alarm_count");
    AddContent(event, "ip", "ignored");
    auto bytes = group.SerializeAsString();
    bytes.append("\x98\x06\x01", 3); // Unknown root field 99, varint 1.
    APSARA_TEST_EQUAL(SelfMonitorServer::GetInstance()->ReceiveGoAlarmsPB(bytes), 1);
    auto alarms = FlushAlarms();
    ASSERT_EQ(1U, alarms.size());
    APSARA_TEST_EQUAL(alarms[0]["alarm_type"], "LAST_TYPE");
    APSARA_TEST_EQUAL(alarms[0]["alarm_count"], "3");
    APSARA_TEST_EQUAL(alarms[0]["alarm_level"], "2");
}

void SelfMonitorServerUnittest::TestReceiveGoAlarmsPBMalformedBatch() {
    auto valid = BuildAlarmsPB({{{"alarm_type", "ATOMIC_BATCH"}, {"alarm_count", "2"}}});
    auto truncated = valid;
    truncated.append("\x1a\x05\x0a", 3); // A valid first message followed by a truncated logs field.
    APSARA_TEST_EQUAL(SelfMonitorServer::GetInstance()->ReceiveGoAlarmsPB(truncated), 0);
    APSARA_TEST_TRUE(FlushAlarms().empty());
    APSARA_TEST_EQUAL(SelfMonitorServer::GetInstance()->ReceiveGoAlarmsPB(valid), 1);
    auto alarms = FlushAlarms();
    ASSERT_EQ(1U, alarms.size());
    APSARA_TEST_EQUAL(alarms[0]["alarm_count"], "2");
}

void SelfMonitorServerUnittest::TestReceiveGoAlarmsPBOneof() {
    auto logs = BuildAlarmsPB({{{"alarm_type", "ONEOF_BATCH"}, {"alarm_count", "1"}}});
    models::PipelineEventGroup metricGroup;
    metricGroup.mutable_metrics()->add_events()->set_timestamp(1);
    auto metrics = metricGroup.SerializeAsString();
    auto* server = SelfMonitorServer::GetInstance();
    APSARA_TEST_EQUAL(server->ReceiveGoAlarmsPB(logs + metrics), 0);
    APSARA_TEST_TRUE(FlushAlarms().empty());
    APSARA_TEST_EQUAL(server->ReceiveGoAlarmsPB(metrics + logs), 1);
    APSARA_TEST_EQUAL(server->ReceiveGoAlarmsPB(logs + logs), 2);
    auto alarms = FlushAlarms();
    ASSERT_EQ(1U, alarms.size());
    APSARA_TEST_EQUAL(alarms[0]["alarm_count"], "3");
}

void SelfMonitorServerUnittest::TestReceiveGoAlarmsPBBatchSizes() {
    auto* server = SelfMonitorServer::GetInstance();
    auto small = BuildAlarmsPB({{{"alarm_type", "SIZE_BATCH"}, {"alarm_count", "1"}}});
    auto large = BuildAlarmsPB(
        {{{"alarm_type", "SIZE_BATCH"}, {"alarm_count", "1"}, {"alarm_message", std::string(128 * 1024, 'x')}}});
    models::PipelineEventGroup expanded;
    for (int i = 0; i < 10000; ++i) {
        expanded.mutable_logs()->add_events();
    }
    auto manyEmpty = expanded.SerializeAsString(); // Small wire payload, large in-memory subtree.
    for (int i = 0; i < 10; ++i) {
        APSARA_TEST_EQUAL(server->ReceiveGoAlarmsPB(large), 1);
        APSARA_TEST_EQUAL(server->ReceiveGoAlarmsPB(small), 1);
        APSARA_TEST_EQUAL(server->ReceiveGoAlarmsPB(manyEmpty), 0);
        APSARA_TEST_EQUAL(server->ReceiveGoAlarmsPB(small), 1);
        APSARA_TEST_EQUAL(server->ReceiveGoAlarmsPB(BuildAlarmsPB({})), 0);
    }
    auto alarms = FlushAlarms();
    ASSERT_EQ(1U, alarms.size());
    APSARA_TEST_EQUAL(alarms[0]["alarm_count"], "30");
}

void SelfMonitorServerUnittest::TestReceiveGoAlarmsPBConcurrent() {
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    for (int thread = 0; thread < 4; ++thread) {
        threads.emplace_back([thread, &failures]() {
            auto bytes = BuildAlarmsPB({{{"alarm_type", "THREAD_BATCH"},
                                         {"alarm_count", "1"},
                                         {"config", "thread-" + std::to_string(thread)}}});
            for (int i = 0; i < 50; ++i) {
                if (SelfMonitorServer::GetInstance()->ReceiveGoAlarmsPB(bytes) != 1) {
                    ++failures;
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    APSARA_TEST_EQUAL(failures.load(), 0);
    auto alarms = FlushAlarms();
    ASSERT_EQ(4U, alarms.size());
    for (const auto& alarm : alarms) {
        APSARA_TEST_EQUAL(alarm.at("alarm_count"), "50");
    }
}

void SelfMonitorServerUnittest::TestReceiveGoAlarmsPBFinalMetadata() {
    // New bridge payloads omit metadata. Old payloads may contain it, but the
    // final sender always supplies its own values instead of forwarding them.
    auto minimal = BuildAlarmsPB({{{"alarm_type", "MINIMAL_METADATA"}, {"alarm_message", "message"}}});
    auto legacy = BuildAlarmsPB({{{"alarm_type", "LEGACY_METADATA"},
                                  {"ip", "ignored-input"},
                                  {"os", "ignored-input"},
                                  {"ver", "ignored-input"}}});
    auto* server = SelfMonitorServer::GetInstance();
    APSARA_TEST_EQUAL(server->ReceiveGoAlarmsPB(minimal), 1);
    APSARA_TEST_EQUAL(server->ReceiveGoAlarmsPB(legacy), 1);
    auto alarms = FlushAlarms();
    ASSERT_EQ(2U, alarms.size());
    for (const auto& alarm : alarms) {
        APSARA_TEST_EQUAL(alarm.at("ip"), LoongCollectorMonitor::mIpAddr);
        APSARA_TEST_FALSE(alarm.at("os").empty());
        APSARA_TEST_FALSE(alarm.at("ver").empty());
        APSARA_TEST_TRUE(alarm.at("os") != "ignored-input");
        APSARA_TEST_TRUE(alarm.at("ver") != "ignored-input");
    }
}

APSARA_UNIT_TEST_CASE(SelfMonitorServerUnittest, TestReceiveGoAlarmsPB, 0);
APSARA_UNIT_TEST_CASE(SelfMonitorServerUnittest, TestReceiveGoAlarmsPBInvalid, 1);
APSARA_UNIT_TEST_CASE(SelfMonitorServerUnittest, TestReceiveGoAlarmsPBCrossBatch, 2);
APSARA_UNIT_TEST_CASE(SelfMonitorServerUnittest, TestReceiveGoAlarmsPBDuplicateFields, 3);
APSARA_UNIT_TEST_CASE(SelfMonitorServerUnittest, TestReceiveGoAlarmsPBMalformedBatch, 4);
APSARA_UNIT_TEST_CASE(SelfMonitorServerUnittest, TestReceiveGoAlarmsPBOneof, 5);
APSARA_UNIT_TEST_CASE(SelfMonitorServerUnittest, TestReceiveGoAlarmsPBBatchSizes, 6);
APSARA_UNIT_TEST_CASE(SelfMonitorServerUnittest, TestReceiveGoAlarmsPBConcurrent, 7);
APSARA_UNIT_TEST_CASE(SelfMonitorServerUnittest, TestReceiveGoAlarmsPBFinalMetadata, 8);

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
