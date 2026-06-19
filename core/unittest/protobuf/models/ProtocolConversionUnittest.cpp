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
#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/PipelineEventGroup.h"
#include "models/SpanEvent.h"
#include "protobuf/models/ProtocolConversion.h"
#include "protobuf/models/pipeline_event_group.pb.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class ProtocolConversionUnittest : public ::testing::Test {
public:
    void TestLogEventRoundTrip();
    void TestMetricEventSingleValueRoundTrip();
    void TestMetricEventMultiValueRoundTrip();
    void TestSpanEventRoundTrip();
    void TestGroupTagsRoundTrip();
    void TestEmptyGroupFails();
};

void ProtocolConversionUnittest::TestLogEventRoundTrip() {
    PipelineEventGroup src(make_shared<SourceBuffer>());
    src.SetTag(string("env"), string("prod"));
    auto* log = src.AddLogEvent();
    log->SetTimestamp(1700000000, 123456789);
    log->SetContent(string("msg"), string("hello log"));
    log->SetContent(string("level"), string("WARN"));

    models::PipelineEventGroup pbGroup;
    string errMsg;
    APSARA_TEST_TRUE(TransferPipelineEventGroupToPB(src, pbGroup, errMsg));

    PipelineEventGroup dst(make_shared<SourceBuffer>());
    APSARA_TEST_TRUE(TransferPBToPipelineEventGroup(pbGroup, dst, errMsg));

    APSARA_TEST_EQUAL(1U, dst.GetEvents().size());
    APSARA_TEST_TRUE(dst.GetEvents()[0].Is<LogEvent>());

    const auto& logOut = dst.GetEvents()[0].Cast<LogEvent>();
    APSARA_TEST_EQUAL(1700000000L, logOut.GetTimestamp());
    APSARA_TEST_EQUAL(string("hello log"), logOut.GetContent("msg").to_string());
    APSARA_TEST_EQUAL(string("WARN"), logOut.GetContent("level").to_string());
    APSARA_TEST_EQUAL(string("prod"), dst.GetTag("env").to_string());
}

void ProtocolConversionUnittest::TestMetricEventSingleValueRoundTrip() {
    PipelineEventGroup src(make_shared<SourceBuffer>());
    auto* metric = src.AddMetricEvent();
    metric->SetName("mem_usage");
    metric->SetTimestamp(1700000001, 0);
    metric->SetValue<UntypedSingleValue>(0.75);
    metric->SetTag(string("host"), string("node1"));

    models::PipelineEventGroup pbGroup;
    string errMsg;
    APSARA_TEST_TRUE(TransferPipelineEventGroupToPB(src, pbGroup, errMsg));

    PipelineEventGroup dst(make_shared<SourceBuffer>());
    APSARA_TEST_TRUE(TransferPBToPipelineEventGroup(pbGroup, dst, errMsg));

    APSARA_TEST_EQUAL(1U, dst.GetEvents().size());
    APSARA_TEST_TRUE(dst.GetEvents()[0].Is<MetricEvent>());

    const auto& metricOut = dst.GetEvents()[0].Cast<MetricEvent>();
    APSARA_TEST_EQUAL(string("mem_usage"), metricOut.GetName().to_string());
    APSARA_TEST_EQUAL(1700000001L, metricOut.GetTimestamp());
    APSARA_TEST_TRUE(metricOut.Is<UntypedSingleValue>());
    APSARA_TEST_EQUAL(0.75, metricOut.GetValue<UntypedSingleValue>()->mValue);
    APSARA_TEST_EQUAL(string("node1"), metricOut.GetTag("host").to_string());
}

void ProtocolConversionUnittest::TestMetricEventMultiValueRoundTrip() {
    PipelineEventGroup src(make_shared<SourceBuffer>());
    auto* metric = src.AddMetricEvent();
    metric->SetName("system_metrics");
    metric->SetTimestamp(1700000002, 0);
    auto* multiValues = metric->MutableValue<UntypedMultiDoubleValues>();
    multiValues->SetValue(string("cpu"), {UntypedValueMetricType::MetricTypeGauge, 0.6});
    multiValues->SetValue(string("memory"), {UntypedValueMetricType::MetricTypeCounter, 1024.0});

    models::PipelineEventGroup pbGroup;
    string errMsg;
    APSARA_TEST_TRUE(TransferPipelineEventGroupToPB(src, pbGroup, errMsg));

    PipelineEventGroup dst(make_shared<SourceBuffer>());
    APSARA_TEST_TRUE(TransferPBToPipelineEventGroup(pbGroup, dst, errMsg));

    APSARA_TEST_EQUAL(1U, dst.GetEvents().size());
    APSARA_TEST_TRUE(dst.GetEvents()[0].Is<MetricEvent>());

    const auto& metricOut = dst.GetEvents()[0].Cast<MetricEvent>();
    APSARA_TEST_EQUAL(string("system_metrics"), metricOut.GetName().to_string());
    APSARA_TEST_TRUE(metricOut.Is<UntypedMultiDoubleValues>());

    const auto* multiOut = metricOut.GetValue<UntypedMultiDoubleValues>();
    APSARA_TEST_NOT_EQUAL(nullptr, multiOut);

    bool foundCpu = false, foundMemory = false;
    for (auto it = multiOut->ValuesBegin(); it != multiOut->ValuesEnd(); ++it) {
        if (it->first.to_string() == "cpu") {
            APSARA_TEST_EQUAL(UntypedValueMetricType::MetricTypeGauge, it->second.MetricType);
            APSARA_TEST_EQUAL(0.6, it->second.Value);
            foundCpu = true;
        } else if (it->first.to_string() == "memory") {
            APSARA_TEST_EQUAL(UntypedValueMetricType::MetricTypeCounter, it->second.MetricType);
            APSARA_TEST_EQUAL(1024.0, it->second.Value);
            foundMemory = true;
        }
    }
    APSARA_TEST_TRUE(foundCpu);
    APSARA_TEST_TRUE(foundMemory);
}

void ProtocolConversionUnittest::TestSpanEventRoundTrip() {
    PipelineEventGroup src(make_shared<SourceBuffer>());
    auto* span = src.AddSpanEvent();
    span->SetTimestamp(1700000003, 0);
    span->SetTraceId("trace-001");
    span->SetSpanId("span-001");
    span->SetName("http.request");
    span->SetKind(SpanEvent::Kind::Server);
    span->SetStartTimeNs(1700000003000000000ULL);
    span->SetEndTimeNs(1700000003500000000ULL);
    span->SetTag(string("http.method"), string("GET"));
    span->SetStatus(SpanEvent::StatusCode::Ok);

    models::PipelineEventGroup pbGroup;
    string errMsg;
    APSARA_TEST_TRUE(TransferPipelineEventGroupToPB(src, pbGroup, errMsg));

    PipelineEventGroup dst(make_shared<SourceBuffer>());
    APSARA_TEST_TRUE(TransferPBToPipelineEventGroup(pbGroup, dst, errMsg));

    APSARA_TEST_EQUAL(1U, dst.GetEvents().size());
    APSARA_TEST_TRUE(dst.GetEvents()[0].Is<SpanEvent>());

    const auto& spanOut = dst.GetEvents()[0].Cast<SpanEvent>();
    APSARA_TEST_EQUAL(string("trace-001"), spanOut.GetTraceId().to_string());
    APSARA_TEST_EQUAL(string("span-001"), spanOut.GetSpanId().to_string());
    APSARA_TEST_EQUAL(string("http.request"), spanOut.GetName().to_string());
    APSARA_TEST_EQUAL(SpanEvent::Kind::Server, spanOut.GetKind());
    APSARA_TEST_EQUAL(1700000003000000000ULL, spanOut.GetStartTimeNs());
    APSARA_TEST_EQUAL(1700000003500000000ULL, spanOut.GetEndTimeNs());
    APSARA_TEST_EQUAL(string("GET"), spanOut.GetTag("http.method").to_string());
    APSARA_TEST_EQUAL(SpanEvent::StatusCode::Ok, spanOut.GetStatus());
}

void ProtocolConversionUnittest::TestGroupTagsRoundTrip() {
    PipelineEventGroup src(make_shared<SourceBuffer>());
    src.SetTag(string("region"), string("cn-hangzhou"));
    src.SetTag(string("app"), string("loongcollector"));

    // Need at least one event
    auto* log = src.AddLogEvent();
    log->SetTimestamp(1700000004, 0);
    log->SetContent(string("k"), string("v"));

    models::PipelineEventGroup pbGroup;
    string errMsg;
    APSARA_TEST_TRUE(TransferPipelineEventGroupToPB(src, pbGroup, errMsg));

    PipelineEventGroup dst(make_shared<SourceBuffer>());
    APSARA_TEST_TRUE(TransferPBToPipelineEventGroup(pbGroup, dst, errMsg));

    APSARA_TEST_EQUAL(string("cn-hangzhou"), dst.GetTag("region").to_string());
    APSARA_TEST_EQUAL(string("loongcollector"), dst.GetTag("app").to_string());
}

void ProtocolConversionUnittest::TestEmptyGroupFails() {
    PipelineEventGroup emptyGroup(make_shared<SourceBuffer>());
    models::PipelineEventGroup pbGroup;
    string errMsg;
    APSARA_TEST_FALSE(TransferPipelineEventGroupToPB(emptyGroup, pbGroup, errMsg));
    APSARA_TEST_FALSE(errMsg.empty());
}

UNIT_TEST_CASE(ProtocolConversionUnittest, TestLogEventRoundTrip)
UNIT_TEST_CASE(ProtocolConversionUnittest, TestMetricEventSingleValueRoundTrip)
UNIT_TEST_CASE(ProtocolConversionUnittest, TestMetricEventMultiValueRoundTrip)
UNIT_TEST_CASE(ProtocolConversionUnittest, TestSpanEventRoundTrip)
UNIT_TEST_CASE(ProtocolConversionUnittest, TestGroupTagsRoundTrip)
UNIT_TEST_CASE(ProtocolConversionUnittest, TestEmptyGroupFails)

} // namespace logtail
