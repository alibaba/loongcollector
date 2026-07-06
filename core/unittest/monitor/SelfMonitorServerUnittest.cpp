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

#include <string>

#include "models/PipelineEventGroup.h"
#include "monitor/SelfMonitorServer.h"
#include "protobuf/models/ProtocolConversion.h"
#include "protobuf/models/pipeline_event_group.pb.h"
#include "unittest/Unittest.h"

namespace logtail {

class SelfMonitorServerUnittest : public ::testing::Test {
public:
    void TestReceiveGoAlarmsPB();
    void TestReceiveGoAlarmsPBInvalid();

private:
    // Serialize a PipelineEventGroup of alarm LogEvents into PB bytes, mirroring the
    // D3 Go push payload produced by helper.TransferAlarmsToPipelineEventGroup.
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
        pb.SerializeToString(&bytes);
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

APSARA_UNIT_TEST_CASE(SelfMonitorServerUnittest, TestReceiveGoAlarmsPB, 0);
APSARA_UNIT_TEST_CASE(SelfMonitorServerUnittest, TestReceiveGoAlarmsPBInvalid, 1);

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
