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

#include "models/PipelineEventGroup.h"
#include "protobuf/models/ProtocolConversion.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

namespace {

void RoundTripGroup(const PipelineEventGroup& src, PipelineEventGroup& dst) {
    models::PipelineEventGroup pb;
    string errMsg;
    ASSERT_TRUE(TransferPipelineEventGroupToPB(src, pb, errMsg)) << errMsg;
    ASSERT_TRUE(TransferPBToPipelineEventGroup(pb, dst, errMsg)) << errMsg;
}

} // namespace

class ProtocolConversionUnittest : public ::testing::Test {
protected:
    void SetUp() override { mSourceBuffer = make_shared<SourceBuffer>(); }

    shared_ptr<SourceBuffer> mSourceBuffer;
};

TEST_F(ProtocolConversionUnittest, LogEventRoundTrip) {
    PipelineEventGroup src(mSourceBuffer);
    auto* log = src.AddLogEvent();
    log->SetTimestamp(100, 200);
    log->SetContent(string("content"), string("hello"));
    log->SetContent(string("k1"), string("v1"));
    log->SetLevel(string("info"));
    log->SetPosition(42, 128);
    src.SetTag(string("pipeline"), string("p1"));
    src.SetMetadata(EventGroupMetaKey::SOURCE_ID, string("pack-1"));

    PipelineEventGroup dst(mSourceBuffer);
    RoundTripGroup(src, dst);

    ASSERT_EQ(1U, dst.GetEvents().size());
    const auto& out = dst.GetEvents()[0].Cast<LogEvent>();
    EXPECT_EQ(100U, out.GetTimestamp());
    EXPECT_EQ(200U, out.GetTimestampNanosecond().value_or(0));
    EXPECT_EQ("hello", out.GetContent("content").to_string());
    EXPECT_EQ("info", out.GetLevel().to_string());
    EXPECT_EQ(42U, out.GetPosition().first);
    EXPECT_EQ("p1", dst.GetTag("pipeline").to_string());
    EXPECT_EQ("pack-1", dst.GetMetadata(EventGroupMetaKey::SOURCE_ID).to_string());
}

TEST_F(ProtocolConversionUnittest, MetricSingleValueRoundTrip) {
    PipelineEventGroup src(mSourceBuffer);
    auto* metric = src.AddMetricEvent();
    metric->SetTimestamp(10, 0);
    metric->SetName("cpu");
    metric->SetValue(UntypedSingleValue{0.5});
    metric->SetTag(string("host"), string("node-a"));

    PipelineEventGroup dst(mSourceBuffer);
    RoundTripGroup(src, dst);

    const auto& out = dst.GetEvents()[0].Cast<MetricEvent>();
    EXPECT_EQ("cpu", out.GetName().to_string());
    ASSERT_TRUE(out.Is<UntypedSingleValue>());
    EXPECT_DOUBLE_EQ(0.5, out.GetValue<UntypedSingleValue>()->mValue);
}

TEST_F(ProtocolConversionUnittest, MetricMultiValueRoundTrip) {
    PipelineEventGroup src(mSourceBuffer);
    auto* metric = src.AddMetricEvent();
    metric->SetTimestamp(20, 0);
    metric->SetName("agent");
    UntypedMultiDoubleValues values({{"cpu", {UntypedValueMetricType::MetricTypeGauge, 0.1}},
                                      {"memory_used_mb", {UntypedValueMetricType::MetricTypeCounter, 25.0}}},
                                    nullptr);
    metric->SetValue(values);

    PipelineEventGroup dst(mSourceBuffer);
    RoundTripGroup(src, dst);

    const auto& out = dst.GetEvents()[0].Cast<MetricEvent>();
    EXPECT_EQ("agent", out.GetName().to_string());
    ASSERT_TRUE(out.Is<UntypedMultiDoubleValues>());
    UntypedMultiDoubleValue field;
    ASSERT_TRUE(out.GetValue<UntypedMultiDoubleValues>()->GetValue("cpu", field));
    EXPECT_DOUBLE_EQ(0.1, field.Value);
    ASSERT_TRUE(out.GetValue<UntypedMultiDoubleValues>()->GetValue("memory_used_mb", field));
    EXPECT_DOUBLE_EQ(25.0, field.Value);
    EXPECT_EQ(2U, std::distance(out.GetValue<UntypedMultiDoubleValues>()->ValuesBegin(),
                                out.GetValue<UntypedMultiDoubleValues>()->ValuesEnd()));
}

TEST_F(ProtocolConversionUnittest, SpanEventRoundTrip) {
    PipelineEventGroup src(mSourceBuffer);
    auto* span = src.AddSpanEvent();
    span->SetTimestamp(1, 500);
    span->SetTraceId("trace-1");
    span->SetSpanId("span-1");
    span->SetParentSpanId("parent-1");
    span->SetTraceState("state");
    span->SetName("op");
    span->SetKind(SpanEvent::Kind::Client);
    span->SetStartTimeNs(1000);
    span->SetEndTimeNs(2000);
    span->SetStatus(SpanEvent::StatusCode::Ok);
    span->SetTag(string("service"), string("svc"));
    span->SetScopeTag(string("scope.k"), string("scope.v"));

    auto* inner = span->AddEvent();
    inner->SetTimestampNs(1500);
    inner->SetName("evt");
    inner->SetTag(string("ekey"), string("eval"));

    auto* link = span->AddLink();
    link->SetTraceId("lt");
    link->SetSpanId("ls");
    link->SetTraceState("lstate");
    link->SetTag(string("lkey"), string("lval"));

    PipelineEventGroup dst(mSourceBuffer);
    RoundTripGroup(src, dst);

    const auto& out = dst.GetEvents()[0].Cast<SpanEvent>();
    EXPECT_EQ("trace-1", out.GetTraceId().to_string());
    EXPECT_EQ("span-1", out.GetSpanId().to_string());
    EXPECT_EQ(SpanEvent::Kind::Client, out.GetKind());
    EXPECT_EQ(1U, out.GetEvents().size());
    EXPECT_EQ("evt", out.GetEvents()[0].GetName().to_string());
    EXPECT_EQ(1U, out.GetLinks().size());
    EXPECT_EQ("scope.v", out.GetScopeTag("scope.k").to_string());
}

TEST_F(ProtocolConversionUnittest, PipelineEventGroupWithMetadataAndTags) {
    PipelineEventGroup src(mSourceBuffer);
    auto* metric = src.AddMetricEvent();
    metric->SetName("m");
    metric->SetValue(UntypedSingleValue{1.0});
    src.SetTag(string("env"), string("prod"));
    src.SetMetadata(EventGroupMetaKey::INTERNAL_DATA_TYPE, string("self_monitor"));

    PipelineEventGroup dst(mSourceBuffer);
    RoundTripGroup(src, dst);

    EXPECT_EQ("prod", dst.GetTag("env").to_string());
    EXPECT_EQ("self_monitor", dst.GetMetadata(EventGroupMetaKey::INTERNAL_DATA_TYPE).to_string());
}

TEST_F(ProtocolConversionUnittest, UnsupportedMixedTypesInGroup) {
    PipelineEventGroup src(mSourceBuffer);
    src.AddLogEvent();
    src.AddMetricEvent();

    models::PipelineEventGroup pb;
    string errMsg;
    EXPECT_FALSE(TransferPipelineEventGroupToPB(src, pb, errMsg));
    EXPECT_FALSE(errMsg.empty());
}

} // namespace logtail

UNIT_TEST_MAIN
