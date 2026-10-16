/*
 * Copyright 2026 iLogtail Authors
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

#include "gtest/gtest.h"

#include "go_pipeline/LogtailPlugin.h"
#include "models/PipelineEventGroup.h"
#include "protobuf/models/ProtocolConversion.h"
#include "protobuf/models/pipeline_event_group.pb.h"
#include "unittest/Unittest.h"
#include "unittest/pipeline/LogtailPluginMock.h"

using namespace std;

namespace logtail {

namespace {

void SendGroupToGoPlugin(const PipelineEventGroup& group, const string& packId) {
    models::PipelineEventGroup pb;
    string errMsg;
    ASSERT_TRUE(TransferPipelineEventGroupToPB(group, pb, errMsg)) << errMsg;
    string serialized;
    ASSERT_TRUE(pb.SerializeToString(&serialized));
    LogtailPlugin::GetInstance()->ProcessPipelineEventGroup("test_config", serialized, packId);
}

string SerializeGroup(const PipelineEventGroup& group) {
    models::PipelineEventGroup pb;
    string errMsg;
    EXPECT_TRUE(TransferPipelineEventGroupToPB(group, pb, errMsg)) << errMsg;
    string serialized;
    EXPECT_TRUE(pb.SerializeToString(&serialized));
    return serialized;
}

int CallSendPipelineEventGroup(const string& configName, const string& serialized, const string& logstore = "ls") {
    return LogtailPlugin::SendPipelineEventGroup(configName.c_str(),
                                                 static_cast<int32_t>(configName.size()),
                                                 logstore.c_str(),
                                                 static_cast<int>(logstore.size()),
                                                 const_cast<char*>(serialized.data()),
                                                 static_cast<int32_t>(serialized.size()),
                                                 1,
                                                 "",
                                                 0);
}

} // namespace

class GoPipelineBridgeUnittest : public ::testing::Test {
protected:
    void SetUp() override {
        mSourceBuffer = make_shared<SourceBuffer>();
        LogtailPluginMock::ResetProcessPipelineEventGroupStats();
    }

    shared_ptr<SourceBuffer> mSourceBuffer;
};

TEST_F(GoPipelineBridgeUnittest, MetricGroupInvokesProcessPipelineEventGroup) {
    PipelineEventGroup group(mSourceBuffer);
    auto* metric = group.AddMetricEvent();
    metric->SetName("agent");
    UntypedMultiDoubleValues multiValues({{"cpu", {UntypedValueMetricType::MetricTypeGauge, 0.2}}}, nullptr);
    metric->SetValue(multiValues);

    SendGroupToGoPlugin(group, "pack-metric");

    EXPECT_EQ(1, LogtailPluginMock::GetProcessPipelineEventGroupCount());
    ASSERT_FALSE(LogtailPluginMock::GetLastPipelineEventGroup().empty());

    models::PipelineEventGroup parsed;
    ASSERT_TRUE(parsed.ParseFromString(LogtailPluginMock::GetLastPipelineEventGroup()));
    EXPECT_EQ(models::PipelineEventGroup::PipelineEventsCase::kMetrics, parsed.PipelineEvents_case());
    EXPECT_EQ(1, parsed.metrics().events_size());
    EXPECT_EQ("agent", parsed.metrics().events(0).name());
}

TEST_F(GoPipelineBridgeUnittest, SpanGroupInvokesProcessPipelineEventGroup) {
    PipelineEventGroup group(mSourceBuffer);
    auto* span = group.AddSpanEvent();
    span->SetName("server-op");
    span->SetTraceId("trace-a");
    span->SetSpanId("span-a");
    span->SetKind(SpanEvent::Kind::Server);

    SendGroupToGoPlugin(group, "pack-span");

    EXPECT_EQ(1, LogtailPluginMock::GetProcessPipelineEventGroupCount());
    models::PipelineEventGroup parsed;
    ASSERT_TRUE(parsed.ParseFromString(LogtailPluginMock::GetLastPipelineEventGroup()));
    EXPECT_EQ(models::PipelineEventGroup::PipelineEventsCase::kSpans, parsed.PipelineEvents_case());
    EXPECT_EQ("server-op", parsed.spans().events(0).name());
}

TEST_F(GoPipelineBridgeUnittest, SendPipelineEventGroupRejectsEmptyBuffer) {
    EXPECT_EQ(-1, LogtailPlugin::SendPipelineEventGroup("cfg", 3, "", 0, nullptr, 0, 0, "", 0));
    char dummy = 0;
    EXPECT_EQ(-1, LogtailPlugin::SendPipelineEventGroup(nullptr, 0, "", 0, &dummy, 1, 0, "", 0));
}

TEST_F(GoPipelineBridgeUnittest, SendPipelineEventGroupRejectsInvalidPB) {
    char junk[] = "not-a-pipeline-event-group-pb";
    EXPECT_EQ(-1,
              LogtailPlugin::SendPipelineEventGroup(
                  "cfg", 3, "ls", 2, junk, static_cast<int32_t>(sizeof(junk) - 1), 1, "", 0));
}

TEST_F(GoPipelineBridgeUnittest, SendPipelineEventGroupMissingConfig) {
    PipelineEventGroup group(mSourceBuffer);
    auto* log = group.AddLogEvent();
    log->SetContent(string("k"), string("v"));
    string serialized = SerializeGroup(group);
    ASSERT_FALSE(serialized.empty());

    EXPECT_EQ(-2, CallSendPipelineEventGroup("no_such_c1_config", serialized));
}

TEST_F(GoPipelineBridgeUnittest, SendPbCoexistsAndReturnsMissingConfig) {
    // C1 must not break the existing SendPb/V2 ABI: unknown config is still -2.
    char dummy[] = "x";
    EXPECT_EQ(
        -2, LogtailPlugin::SendPb("no_such_c1_sendpb", 16, "ls", 2, dummy, static_cast<int32_t>(sizeof(dummy) - 1), 1));
    EXPECT_EQ(-2,
              LogtailPlugin::SendPbV2(
                  "no_such_c1_sendpbv2", 18, "ls", 2, dummy, static_cast<int32_t>(sizeof(dummy) - 1), 1, "", 0));
}

TEST_F(GoPipelineBridgeUnittest, SendPipelineEventGroupAcceptsMetricAndSpanPB) {
    {
        PipelineEventGroup group(mSourceBuffer);
        auto* metric = group.AddMetricEvent();
        metric->SetName("cpu");
        metric->SetValue(UntypedSingleValue{0.5});
        string serialized = SerializeGroup(group);
        ASSERT_FALSE(serialized.empty());
        EXPECT_EQ(-2, CallSendPipelineEventGroup("no_such_c1_metric_config", serialized));
    }
    {
        PipelineEventGroup group(mSourceBuffer);
        auto* span = group.AddSpanEvent();
        span->SetName("op");
        span->SetTraceId("t");
        span->SetSpanId("s");
        string serialized = SerializeGroup(group);
        ASSERT_FALSE(serialized.empty());
        EXPECT_EQ(-2, CallSendPipelineEventGroup("no_such_c1_span_config", serialized));
    }
}

TEST_F(GoPipelineBridgeUnittest, SendPipelineEventGroupContainerConfigEmptyProject) {
    PipelineEventGroup group(mSourceBuffer);
    auto* log = group.AddLogEvent();
    log->SetContent(string("k"), string("v"));
    string serialized = SerializeGroup(group);
    ASSERT_FALSE(serialized.empty());

    // Matches LogtailPlugin constructor: mPluginContainerConfig.mLogstore = "logtail_containers".
    // C1 accepts the PEG and no-ops the profile flusher (still on SendPb).
    EXPECT_EQ(0, CallSendPipelineEventGroup("logtail_containers", serialized));
}

} // namespace logtail

UNIT_TEST_MAIN
