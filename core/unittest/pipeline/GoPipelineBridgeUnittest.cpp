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

#include <memory>
#include <string>

#include "common/memory/SourceBuffer.h"
#include "models/MetricEvent.h"
#include "models/PipelineEventGroup.h"
#include "models/SpanEvent.h"
#include "protobuf/models/ProtocolConversion.h"
#include "protobuf/models/pipeline_event_group.pb.h"
#include "unittest/Unittest.h"
#include "unittest/pipeline/LogtailPluginMock.h"

using namespace std;

namespace logtail {

class GoPipelineBridgeUnittest : public ::testing::Test {
public:
    void TestProcessPipelineEventGroupMockBasic();
    void TestMetricEventGroupSerializeAndBridge();
    void TestLogEventGroupSerializeAndBridge();

protected:
    void SetUp() override { LogtailPluginMock::GetInstance()->ResetPipelineEventGroupCounters(); }
};

void GoPipelineBridgeUnittest::TestProcessPipelineEventGroupMockBasic() {
    const std::string configName = "test_config";
    const std::string packId = "test_pack_id";
    const std::string payload = "test_payload_bytes";

    LogtailPluginMock::GetInstance()->ProcessPipelineEventGroup(configName, payload, packId);

    APSARA_TEST_EQUAL(1, LogtailPluginMock::GetInstance()->GetProcessPipelineEventGroupCallCount());
    APSARA_TEST_EQUAL(payload, LogtailPluginMock::GetInstance()->GetLastPipelineEventGroup());
}

void GoPipelineBridgeUnittest::TestMetricEventGroupSerializeAndBridge() {
    // Build a C++ PipelineEventGroup with a metric event
    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetTag(string("host"), string("localhost"));
    auto* metric = group.AddMetricEvent();
    metric->SetName("cpu_usage");
    metric->SetTimestamp(1000000000, 0);
    metric->SetValue<UntypedSingleValue>(0.85);
    metric->SetTag(string("region"), string("us-east-1"));

    // Serialize to PB
    models::PipelineEventGroup pbGroup;
    string errMsg;
    APSARA_TEST_TRUE(TransferPipelineEventGroupToPB(group, pbGroup, errMsg));

    string serialized;
    APSARA_TEST_TRUE(pbGroup.SerializeToString(&serialized));
    APSARA_TEST_FALSE(serialized.empty());

    // Pass through bridge mock
    LogtailPluginMock::GetInstance()->ProcessPipelineEventGroup("cfg", serialized, "pack1");
    APSARA_TEST_EQUAL(1, LogtailPluginMock::GetInstance()->GetProcessPipelineEventGroupCallCount());
    APSARA_TEST_EQUAL(serialized, LogtailPluginMock::GetInstance()->GetLastPipelineEventGroup());

    // Deserialize back and verify
    models::PipelineEventGroup pbGroupOut;
    APSARA_TEST_TRUE(pbGroupOut.ParseFromString(serialized));
    PipelineEventGroup groupOut(make_shared<SourceBuffer>());
    APSARA_TEST_TRUE(TransferPBToPipelineEventGroup(pbGroupOut, groupOut, errMsg));

    APSARA_TEST_EQUAL(1U, groupOut.GetEvents().size());
    APSARA_TEST_TRUE(groupOut.GetEvents()[0].Is<MetricEvent>());
    const auto& metricOut = groupOut.GetEvents()[0].Cast<MetricEvent>();
    APSARA_TEST_EQUAL(string("cpu_usage"), metricOut.GetName().to_string());
    APSARA_TEST_TRUE(metricOut.Is<UntypedSingleValue>());
    APSARA_TEST_EQUAL(0.85, metricOut.GetValue<UntypedSingleValue>()->mValue);
    APSARA_TEST_EQUAL(string("us-east-1"), groupOut.GetEvents()[0].Cast<MetricEvent>().GetTag("region").to_string());
    APSARA_TEST_EQUAL(string("localhost"), groupOut.GetTag("host").to_string());
}

void GoPipelineBridgeUnittest::TestLogEventGroupSerializeAndBridge() {
    // Build a C++ PipelineEventGroup with a log event
    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetTag(string("source"), string("stdin"));
    auto* log = group.AddLogEvent();
    log->SetTimestamp(2000000000, 500000000);
    log->SetContent(string("message"), string("hello world"));
    log->SetContent(string("level"), string("INFO"));

    // Serialize to PB
    models::PipelineEventGroup pbGroup;
    string errMsg;
    APSARA_TEST_TRUE(TransferPipelineEventGroupToPB(group, pbGroup, errMsg));

    string serialized;
    APSARA_TEST_TRUE(pbGroup.SerializeToString(&serialized));
    APSARA_TEST_FALSE(serialized.empty());

    // Pass through bridge mock
    LogtailPluginMock::GetInstance()->ProcessPipelineEventGroup("cfg2", serialized, "pack2");
    APSARA_TEST_EQUAL(1, LogtailPluginMock::GetInstance()->GetProcessPipelineEventGroupCallCount());

    // Deserialize back and verify
    models::PipelineEventGroup pbGroupOut;
    APSARA_TEST_TRUE(pbGroupOut.ParseFromString(serialized));
    PipelineEventGroup groupOut(make_shared<SourceBuffer>());
    APSARA_TEST_TRUE(TransferPBToPipelineEventGroup(pbGroupOut, groupOut, errMsg));

    APSARA_TEST_EQUAL(1U, groupOut.GetEvents().size());
    APSARA_TEST_TRUE(groupOut.GetEvents()[0].Is<LogEvent>());
    const auto& logOut = groupOut.GetEvents()[0].Cast<LogEvent>();
    APSARA_TEST_EQUAL(string("hello world"), logOut.GetContent("message").to_string());
    APSARA_TEST_EQUAL(string("stdin"), groupOut.GetTag("source").to_string());
}

UNIT_TEST_CASE(GoPipelineBridgeUnittest, TestProcessPipelineEventGroupMockBasic)
UNIT_TEST_CASE(GoPipelineBridgeUnittest, TestMetricEventGroupSerializeAndBridge)
UNIT_TEST_CASE(GoPipelineBridgeUnittest, TestLogEventGroupSerializeAndBridge)

} // namespace logtail
