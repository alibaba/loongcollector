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

} // namespace logtail

UNIT_TEST_MAIN
