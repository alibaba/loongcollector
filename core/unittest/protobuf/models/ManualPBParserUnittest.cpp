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

#include <memory>
#include <string>
#include <vector>

#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/PipelineEventGroup.h"
#include "models/SpanEvent.h"
#include "protobuf/models/ManualPBParser.h"
#include "protobuf/models/log_event.pb.h"
#include "protobuf/models/metric_event.pb.h"
#include "protobuf/models/pipeline_event_group.pb.h"
#include "protobuf/models/span_event.pb.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class ManualPBParserUnittest : public ::testing::Test {
public:
    void TestParseEmptyData();
    void TestParseNullData();
    void TestParseInvalidData();
    void TestParseLogEventWithContent();
    void TestParseLogEventWithMultipleContents();
    void TestParseLogEventWithLevel();
    void TestParseLogEventWithPosition();
    void TestParseMetricEvent();
    void TestParseMetricEventWithTags();
    void TestParseSpanEvent();
    void TestParseSpanEventWithTags();
    void TestParseSpanEventWithScopeTags();
    void TestParseSpanEventWithStatus();
    void TestParseSpanEventWithInnerEvents();
    void TestParseSpanEventWithInnerEventTags();
    void TestParseSpanEventWithLinks();
    void TestParseSpanEventWithLinkTraceStateAndTags();
    void TestParseSpanEventWithAllFields();
    void TestParsePipelineEventGroupWithMetadata();
    void TestParsePipelineEventGroupWithTags();
    void TestParseInvalidWireType();
    void TestParseIncompletePB();
    void TestParseUnknownFields();
    void TestParseReplaceSpanTags();
    void TestParseLargeVarint();
    void TestParseMultipleLogEvents();
    void TestParseMultipleMetricEvents();
    void TestParseMultipleSpanEvents();

protected:
    void SetUp() override {
        mSourceBuffer.reset(new SourceBuffer);
        mEventGroup.reset(new PipelineEventGroup(mSourceBuffer));
    }

private:
    // Helper to serialize PB EventGroup to binary
    std::vector<uint8_t> serializePBEventGroup(const models::PipelineEventGroup& pbGroup);

    shared_ptr<SourceBuffer> mSourceBuffer;
    unique_ptr<PipelineEventGroup> mEventGroup;
};

std::vector<uint8_t> ManualPBParserUnittest::serializePBEventGroup(const models::PipelineEventGroup& pbGroup) {
    std::string serialized = pbGroup.SerializeAsString();
    return std::vector<uint8_t>(serialized.begin(), serialized.end());
}

void ManualPBParserUnittest::TestParseEmptyData() {
    std::vector<uint8_t> emptyData;
    ManualPBParser parser(emptyData.data(), emptyData.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_FALSE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE_FATAL(!errMsg.empty());
}

void ManualPBParserUnittest::TestParseNullData() {
    ManualPBParser parser(nullptr, 0);
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_FALSE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE_FATAL(!errMsg.empty());
}

void ManualPBParserUnittest::TestParseInvalidData() {
    // Create invalid protobuf data
    std::vector<uint8_t> invalidData = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ManualPBParser parser(invalidData.data(), invalidData.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_FALSE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE_FATAL(!errMsg.empty());
}

void ManualPBParserUnittest::TestParseLogEventWithContent() {
    // Create PB LogEvent
    models::PipelineEventGroup pbGroup;
    auto* pbLogEvents = pbGroup.mutable_logs();
    auto* pbLog = pbLogEvents->add_events();
    pbLog->set_timestamp(1234567890);
    auto* content = pbLog->add_contents();
    content->set_key("log_key");
    content->set_value("log_value");

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(1U, eventGroup.GetEvents().size());

    const auto& event = eventGroup.GetEvents()[0];
    APSARA_TEST_TRUE_FATAL(event.Is<LogEvent>());
    const auto& logEvent = event.Cast<LogEvent>();
    APSARA_TEST_EQUAL_FATAL(1234567890U, logEvent.GetTimestamp());
    APSARA_TEST_TRUE_FATAL(logEvent.HasContent("log_key"));
    APSARA_TEST_EQUAL_FATAL("log_value", logEvent.GetContent("log_key").to_string());
}

void ManualPBParserUnittest::TestParseLogEventWithMultipleContents() {
    // Create PB LogEvent with multiple contents
    models::PipelineEventGroup pbGroup;
    auto* pbLogEvents = pbGroup.mutable_logs();
    auto* pbLog = pbLogEvents->add_events();
    pbLog->set_timestamp(9876543210ULL);

    auto* content1 = pbLog->add_contents();
    content1->set_key("key1");
    content1->set_value("value1");

    auto* content2 = pbLog->add_contents();
    content2->set_key("key2");
    content2->set_value("value2");

    auto* content3 = pbLog->add_contents();
    content3->set_key("key3");
    content3->set_value("value3");

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(1U, eventGroup.GetEvents().size());

    const auto& event = eventGroup.GetEvents()[0];
    APSARA_TEST_TRUE_FATAL(event.Is<LogEvent>());
    const auto& logEvent = event.Cast<LogEvent>();
    APSARA_TEST_EQUAL_FATAL(9876543210ULL, logEvent.GetTimestamp());
    APSARA_TEST_TRUE_FATAL(logEvent.HasContent("key1"));
    APSARA_TEST_EQUAL_FATAL("value1", logEvent.GetContent("key1").to_string());
    APSARA_TEST_TRUE_FATAL(logEvent.HasContent("key2"));
    APSARA_TEST_EQUAL_FATAL("value2", logEvent.GetContent("key2").to_string());
    APSARA_TEST_TRUE_FATAL(logEvent.HasContent("key3"));
    APSARA_TEST_EQUAL_FATAL("value3", logEvent.GetContent("key3").to_string());
}

void ManualPBParserUnittest::TestParseLogEventWithLevel() {
    // Create PB LogEvent with level
    models::PipelineEventGroup pbGroup;
    auto* pbLogEvents = pbGroup.mutable_logs();
    auto* pbLog = pbLogEvents->add_events();
    pbLog->set_timestamp(1111111111);
    pbLog->set_level("INFO");

    auto* content = pbLog->add_contents();
    content->set_key("message");
    content->set_value("test message");

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(1U, eventGroup.GetEvents().size());

    const auto& event = eventGroup.GetEvents()[0];
    APSARA_TEST_TRUE_FATAL(event.Is<LogEvent>());
    const auto& logEvent = event.Cast<LogEvent>();
    APSARA_TEST_EQUAL_FATAL("INFO", logEvent.GetLevel().to_string());
}

void ManualPBParserUnittest::TestParseLogEventWithPosition() {
    // Create PB LogEvent with position
    models::PipelineEventGroup pbGroup;
    auto* pbLogEvents = pbGroup.mutable_logs();
    auto* pbLog = pbLogEvents->add_events();
    pbLog->set_timestamp(2222222222);
    pbLog->set_fileoffset(12345);
    pbLog->set_rawsize(67890);

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(1U, eventGroup.GetEvents().size());

    const auto& event = eventGroup.GetEvents()[0];
    APSARA_TEST_TRUE_FATAL(event.Is<LogEvent>());
    const auto& logEvent = event.Cast<LogEvent>();
    APSARA_TEST_EQUAL_FATAL(12345U, logEvent.GetPosition().first);
    APSARA_TEST_EQUAL_FATAL(67890U, logEvent.GetPosition().second);
}

void ManualPBParserUnittest::TestParseMetricEvent() {
    // Create PB MetricEvent
    models::PipelineEventGroup pbGroup;
    auto* pbMetricEvents = pbGroup.mutable_metrics();
    auto* pbMetric = pbMetricEvents->add_events();
    pbMetric->set_timestamp(3333333333ULL);
    pbMetric->set_name("cpu_usage");
    pbMetric->mutable_untypedsinglevalue()->set_value(0.75);

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(1U, eventGroup.GetEvents().size());

    const auto& event = eventGroup.GetEvents()[0];
    APSARA_TEST_TRUE_FATAL(event.Is<MetricEvent>());
    const auto& metricEvent = event.Cast<MetricEvent>();
    APSARA_TEST_EQUAL_FATAL(3333333333ULL, metricEvent.GetTimestamp());
    APSARA_TEST_EQUAL_FATAL("cpu_usage", metricEvent.GetName().to_string());
    APSARA_TEST_TRUE_FATAL(metricEvent.Is<UntypedSingleValue>());
    APSARA_TEST_EQUAL_FATAL(0.75, metricEvent.GetValue<UntypedSingleValue>()->mValue);
}

void ManualPBParserUnittest::TestParseMetricEventWithTags() {
    // Create PB MetricEvent with tags
    models::PipelineEventGroup pbGroup;
    auto* pbMetricEvents = pbGroup.mutable_metrics();
    auto* pbMetric = pbMetricEvents->add_events();
    pbMetric->set_timestamp(4444444444ULL);
    pbMetric->set_name("memory_usage");
    pbMetric->mutable_untypedsinglevalue()->set_value(1024.0);

    (*pbMetric->mutable_tags())["host"] = "server01";
    (*pbMetric->mutable_tags())["region"] = "us-west";

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(1U, eventGroup.GetEvents().size());

    const auto& event = eventGroup.GetEvents()[0];
    APSARA_TEST_TRUE_FATAL(event.Is<MetricEvent>());
    const auto& metricEvent = event.Cast<MetricEvent>();
    APSARA_TEST_EQUAL_FATAL("memory_usage", metricEvent.GetName().to_string());
    APSARA_TEST_TRUE_FATAL(metricEvent.HasTag("host"));
    APSARA_TEST_EQUAL_FATAL("server01", metricEvent.GetTag("host").to_string());
    APSARA_TEST_TRUE_FATAL(metricEvent.HasTag("region"));
    APSARA_TEST_EQUAL_FATAL("us-west", metricEvent.GetTag("region").to_string());
}

void ManualPBParserUnittest::TestParseSpanEvent() {
    // Create PB SpanEvent
    models::PipelineEventGroup pbGroup;
    auto* pbSpanEvents = pbGroup.mutable_spans();
    auto* pbSpan = pbSpanEvents->add_events();
    pbSpan->set_timestamp(5555555555ULL);
    pbSpan->set_traceid("trace123");
    pbSpan->set_spanid("span456");
    pbSpan->set_parentspanid("parent789");
    pbSpan->set_name("test_span");
    pbSpan->set_kind(models::SpanEvent::SERVER);
    pbSpan->set_starttime(6666666666ULL);
    pbSpan->set_endtime(7777777777ULL);
    pbSpan->set_status(models::SpanEvent::Ok);

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(1U, eventGroup.GetEvents().size());

    const auto& event = eventGroup.GetEvents()[0];
    APSARA_TEST_TRUE_FATAL(event.Is<SpanEvent>());
    const auto& spanEvent = event.Cast<SpanEvent>();
    APSARA_TEST_EQUAL_FATAL("trace123", spanEvent.GetTraceId().to_string());
    APSARA_TEST_EQUAL_FATAL("span456", spanEvent.GetSpanId().to_string());
    APSARA_TEST_EQUAL_FATAL("parent789", spanEvent.GetParentSpanId().to_string());
    APSARA_TEST_EQUAL_FATAL("test_span", spanEvent.GetName().to_string());
    APSARA_TEST_EQUAL_FATAL(SpanEvent::Kind::Server, spanEvent.GetKind());
    APSARA_TEST_EQUAL_FATAL(6666666666ULL, spanEvent.GetStartTimeNs());
    APSARA_TEST_EQUAL_FATAL(7777777777ULL, spanEvent.GetEndTimeNs());
}

void ManualPBParserUnittest::TestParseSpanEventWithTags() {
    // Create PB SpanEvent with tags
    models::PipelineEventGroup pbGroup;
    auto* pbSpanEvents = pbGroup.mutable_spans();
    auto* pbSpan = pbSpanEvents->add_events();
    pbSpan->set_traceid("trace_with_tags");
    pbSpan->set_spanid("span_with_tags");
    pbSpan->set_name("tagged_span");
    pbSpan->set_starttime(8888888888ULL);
    pbSpan->set_endtime(9999999999ULL);

    (*pbSpan->mutable_tags())["http.method"] = "GET";
    (*pbSpan->mutable_tags())["http.status_code"] = "200";

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(1U, eventGroup.GetEvents().size());

    const auto& event = eventGroup.GetEvents()[0];
    APSARA_TEST_TRUE_FATAL(event.Is<SpanEvent>());
    const auto& spanEvent = event.Cast<SpanEvent>();
    APSARA_TEST_TRUE_FATAL(spanEvent.HasTag("http.method"));
    APSARA_TEST_EQUAL_FATAL("GET", spanEvent.GetTag("http.method").to_string());
    APSARA_TEST_TRUE_FATAL(spanEvent.HasTag("http.status_code"));
    APSARA_TEST_EQUAL_FATAL("200", spanEvent.GetTag("http.status_code").to_string());
}

void ManualPBParserUnittest::TestParseSpanEventWithScopeTags() {
    // Create PB SpanEvent with scope tags
    models::PipelineEventGroup pbGroup;
    auto* pbSpanEvents = pbGroup.mutable_spans();
    auto* pbSpan = pbSpanEvents->add_events();
    pbSpan->set_traceid("trace_with_scope");
    pbSpan->set_spanid("span_with_scope");
    pbSpan->set_name("scoped_span");
    pbSpan->set_starttime(1111122222ULL);
    pbSpan->set_endtime(3333344444ULL);

    (*pbSpan->mutable_scopetags())["otel.scope.name"] = "io.opentelemetry.http";
    (*pbSpan->mutable_scopetags())["otel.scope.version"] = "1.0.0";

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(1U, eventGroup.GetEvents().size());

    const auto& event = eventGroup.GetEvents()[0];
    APSARA_TEST_TRUE_FATAL(event.Is<SpanEvent>());
    const auto& spanEvent = event.Cast<SpanEvent>();
    APSARA_TEST_TRUE_FATAL(spanEvent.HasScopeTag("otel.scope.name"));
    APSARA_TEST_EQUAL_FATAL("io.opentelemetry.http", spanEvent.GetScopeTag("otel.scope.name").to_string());
    APSARA_TEST_TRUE_FATAL(spanEvent.HasScopeTag("otel.scope.version"));
    APSARA_TEST_EQUAL_FATAL("1.0.0", spanEvent.GetScopeTag("otel.scope.version").to_string());
}

void ManualPBParserUnittest::TestParsePipelineEventGroupWithMetadata() {
    // Create PB EventGroup with metadata
    models::PipelineEventGroup pbGroup;
    (*pbGroup.mutable_metadata())["project"] = "test_project";
    (*pbGroup.mutable_metadata())["logstore"] = "test_logstore";

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE_FATAL(eventGroup.HasTag("project"));
    APSARA_TEST_EQUAL_FATAL("test_project", eventGroup.GetTag("project").to_string());
    APSARA_TEST_TRUE_FATAL(eventGroup.HasTag("logstore"));
    APSARA_TEST_EQUAL_FATAL("test_logstore", eventGroup.GetTag("logstore").to_string());
}

void ManualPBParserUnittest::TestParsePipelineEventGroupWithTags() {
    // Create PB EventGroup with tags
    models::PipelineEventGroup pbGroup;
    (*pbGroup.mutable_tags())["env"] = "production";
    (*pbGroup.mutable_tags())["version"] = "1.0.0";

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE_FATAL(eventGroup.HasTag("env"));
    APSARA_TEST_EQUAL_FATAL("production", eventGroup.GetTag("env").to_string());
    APSARA_TEST_TRUE_FATAL(eventGroup.HasTag("version"));
    APSARA_TEST_EQUAL_FATAL("1.0.0", eventGroup.GetTag("version").to_string());
}


void ManualPBParserUnittest::TestParseInvalidWireType() {
    // Create data with invalid wire type for a field
    std::vector<uint8_t> data = {
        0x08, // field 1 (metadata), wire type 0 (varint) - but metadata should be length-delimited (2)
        0x01 // value
    };

    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_FALSE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE_FATAL(!errMsg.empty());
}

void ManualPBParserUnittest::TestParseIncompletePB() {
    // Create incomplete protobuf data
    std::vector<uint8_t> data = {
        0x0A, // field 1 (metadata), wire type 2 (length-delimited)
        0x05 // length = 5, but no following data
    };

    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_FALSE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE_FATAL(!errMsg.empty());
}

void ManualPBParserUnittest::TestParseUnknownFields() {
    // Create PB with unknown field numbers
    models::PipelineEventGroup pbGroup;
    auto* pbLogEvents = pbGroup.mutable_logs();
    auto* pbLog = pbLogEvents->add_events();
    pbLog->set_timestamp(5000);

    // Serialize to string and add unknown field
    std::string serialized = pbGroup.SerializeAsString();
    std::vector<uint8_t> data(serialized.begin(), serialized.end());

    // Add unknown field (field number 100, varint, value 1)
    data.push_back(0xE0); // field 100, wire type 0
    data.push_back(0x06);
    data.push_back(0x01); // value 1

    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    // Should succeed and skip unknown fields
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

void ManualPBParserUnittest::TestParseReplaceSpanTags() {
    // Create PB SpanEvent with tags
    models::PipelineEventGroup pbGroup;
    auto* pbSpanEvents = pbGroup.mutable_spans();
    auto* pbSpan = pbSpanEvents->add_events();
    pbSpan->set_traceid("trace_replace");
    pbSpan->set_spanid("span_replace");
    pbSpan->set_name("replace_span");
    pbSpan->set_starttime(1234567890ULL);
    pbSpan->set_endtime(1234567900ULL);
    (*pbSpan->mutable_tags())["tag1"] = "value1";

    // Test with replaceSpanTags = true (default)
    {
        auto data = serializePBEventGroup(pbGroup);
        ManualPBParser parser(data.data(), data.size(), true);
        std::string errMsg;

        PipelineEventGroup eventGroup(mSourceBuffer);
        APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
        APSARA_TEST_EQUAL_FATAL(1U, eventGroup.GetEvents().size());
    }

    // Test with replaceSpanTags = false
    {
        auto data = serializePBEventGroup(pbGroup);
        ManualPBParser parser(data.data(), data.size(), false);
        std::string errMsg;

        PipelineEventGroup eventGroup(mSourceBuffer);
        APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
        APSARA_TEST_EQUAL_FATAL(1U, eventGroup.GetEvents().size());
    }
}

void ManualPBParserUnittest::TestParseLargeVarint() {
    // Create PB with large varint values
    models::PipelineEventGroup pbGroup;
    auto* pbLogEvents = pbGroup.mutable_logs();
    auto* pbLog = pbLogEvents->add_events();
    pbLog->set_timestamp(18446744073709551615ULL); // Max uint64
    pbLog->set_fileoffset(9223372036854775807ULL); // Max int64
    pbLog->set_rawsize(18446744073709551614ULL); // Near max uint64

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(1U, eventGroup.GetEvents().size());

    const auto& event = eventGroup.GetEvents()[0];
    APSARA_TEST_TRUE_FATAL(event.Is<LogEvent>());
    const auto& logEvent = event.Cast<LogEvent>();
    APSARA_TEST_EQUAL_FATAL(18446744073709551615ULL, logEvent.GetTimestamp());
}

void ManualPBParserUnittest::TestParseMultipleLogEvents() {
    // Create PB EventGroup with multiple LogEvents
    models::PipelineEventGroup pbGroup;
    auto* pbLogEvents = pbGroup.mutable_logs();

    for (int i = 0; i < 5; ++i) {
        auto* pbLog = pbLogEvents->add_events();
        pbLog->set_timestamp(1000 + i);
        auto* content = pbLog->add_contents();
        content->set_key("log_key");
        content->set_value("log_value_" + std::to_string(i));
    }

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(5U, eventGroup.GetEvents().size());

    for (size_t i = 0; i < 5; ++i) {
        const auto& event = eventGroup.GetEvents()[i];
        APSARA_TEST_TRUE_FATAL(event.Is<LogEvent>());
        const auto& logEvent = event.Cast<LogEvent>();
        APSARA_TEST_EQUAL_FATAL(1000U + i, logEvent.GetTimestamp());
        APSARA_TEST_EQUAL_FATAL("log_value_" + std::to_string(i), logEvent.GetContent("log_key").to_string());
    }
}

void ManualPBParserUnittest::TestParseMultipleMetricEvents() {
    // Create PB EventGroup with multiple MetricEvents
    models::PipelineEventGroup pbGroup;
    auto* pbMetricEvents = pbGroup.mutable_metrics();

    for (int i = 0; i < 3; ++i) {
        auto* pbMetric = pbMetricEvents->add_events();
        pbMetric->set_timestamp(2000 + i);
        pbMetric->set_name("metric_" + std::to_string(i));
        pbMetric->mutable_untypedsinglevalue()->set_value(100.0 + i);
    }

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(3U, eventGroup.GetEvents().size());

    for (size_t i = 0; i < 3; ++i) {
        const auto& event = eventGroup.GetEvents()[i];
        APSARA_TEST_TRUE_FATAL(event.Is<MetricEvent>());
        const auto& metricEvent = event.Cast<MetricEvent>();
        APSARA_TEST_EQUAL_FATAL(2000U + i, metricEvent.GetTimestamp());
        APSARA_TEST_EQUAL_FATAL("metric_" + std::to_string(i), metricEvent.GetName().to_string());
        APSARA_TEST_EQUAL_FATAL(100.0 + i, metricEvent.GetValue<UntypedSingleValue>()->mValue);
    }
}

void ManualPBParserUnittest::TestParseMultipleSpanEvents() {
    // Create PB EventGroup with multiple SpanEvents
    models::PipelineEventGroup pbGroup;
    auto* pbSpanEvents = pbGroup.mutable_spans();

    for (int i = 0; i < 4; ++i) {
        auto* pbSpan = pbSpanEvents->add_events();
        pbSpan->set_traceid("trace_" + std::to_string(i));
        pbSpan->set_spanid("span_" + std::to_string(i));
        pbSpan->set_name("span_name_" + std::to_string(i));
        pbSpan->set_starttime(3000 + i);
        pbSpan->set_endtime(4000 + i);
    }

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(4U, eventGroup.GetEvents().size());

    for (size_t i = 0; i < 4; ++i) {
        const auto& event = eventGroup.GetEvents()[i];
        APSARA_TEST_TRUE_FATAL(event.Is<SpanEvent>());
        const auto& spanEvent = event.Cast<SpanEvent>();
        APSARA_TEST_EQUAL_FATAL("trace_" + std::to_string(i), spanEvent.GetTraceId().to_string());
        APSARA_TEST_EQUAL_FATAL("span_" + std::to_string(i), spanEvent.GetSpanId().to_string());
        APSARA_TEST_EQUAL_FATAL("span_name_" + std::to_string(i), spanEvent.GetName().to_string());
    }
}

void ManualPBParserUnittest::TestParseSpanEventWithStatus() {
    // Create PB SpanEvent with different status codes
    models::PipelineEventGroup pbGroup;
    auto* pbSpanEvents = pbGroup.mutable_spans();

    // Test Unset status
    auto* pbSpan1 = pbSpanEvents->add_events();
    pbSpan1->set_traceid("trace_status_unset");
    pbSpan1->set_spanid("span_status_unset");
    pbSpan1->set_name("span_unset");
    pbSpan1->set_starttime(1000);
    pbSpan1->set_endtime(2000);
    pbSpan1->set_status(models::SpanEvent::Unset);

    // Test Ok status
    auto* pbSpan2 = pbSpanEvents->add_events();
    pbSpan2->set_traceid("trace_status_ok");
    pbSpan2->set_spanid("span_status_ok");
    pbSpan2->set_name("span_ok");
    pbSpan2->set_starttime(3000);
    pbSpan2->set_endtime(4000);
    pbSpan2->set_status(models::SpanEvent::Ok);

    // Test Error status
    auto* pbSpan3 = pbSpanEvents->add_events();
    pbSpan3->set_traceid("trace_status_error");
    pbSpan3->set_spanid("span_status_error");
    pbSpan3->set_name("span_error");
    pbSpan3->set_starttime(5000);
    pbSpan3->set_endtime(6000);
    pbSpan3->set_status(models::SpanEvent::Error);

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(3U, eventGroup.GetEvents().size());

    // Verify Unset status
    const auto& event1 = eventGroup.GetEvents()[0];
    APSARA_TEST_TRUE_FATAL(event1.Is<SpanEvent>());
    const auto& spanEvent1 = event1.Cast<SpanEvent>();
    APSARA_TEST_EQUAL_FATAL(SpanEvent::StatusCode::Unset, spanEvent1.GetStatus());

    // Verify Ok status
    const auto& event2 = eventGroup.GetEvents()[1];
    APSARA_TEST_TRUE_FATAL(event2.Is<SpanEvent>());
    const auto& spanEvent2 = event2.Cast<SpanEvent>();
    APSARA_TEST_EQUAL_FATAL(SpanEvent::StatusCode::Ok, spanEvent2.GetStatus());

    // Verify Error status
    const auto& event3 = eventGroup.GetEvents()[2];
    APSARA_TEST_TRUE_FATAL(event3.Is<SpanEvent>());
    const auto& spanEvent3 = event3.Cast<SpanEvent>();
    APSARA_TEST_EQUAL_FATAL(SpanEvent::StatusCode::Error, spanEvent3.GetStatus());
}

void ManualPBParserUnittest::TestParseSpanEventWithInnerEvents() {
    // Create PB SpanEvent with inner events
    models::PipelineEventGroup pbGroup;
    auto* pbSpanEvents = pbGroup.mutable_spans();
    auto* pbSpan = pbSpanEvents->add_events();
    pbSpan->set_traceid("trace_with_events");
    pbSpan->set_spanid("span_with_events");
    pbSpan->set_name("span_with_events");
    pbSpan->set_starttime(1111111111ULL);
    pbSpan->set_endtime(2222222222ULL);

    // Add first inner event
    auto* innerEvent1 = pbSpan->add_events();
    innerEvent1->set_timestamp(1111111500ULL);
    innerEvent1->set_name("event1");

    // Add second inner event
    auto* innerEvent2 = pbSpan->add_events();
    innerEvent2->set_timestamp(1111111800ULL);
    innerEvent2->set_name("event2");

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(1U, eventGroup.GetEvents().size());

    const auto& event = eventGroup.GetEvents()[0];
    APSARA_TEST_TRUE_FATAL(event.Is<SpanEvent>());
    const auto& spanEvent = event.Cast<SpanEvent>();

    // Verify inner events
    const auto& events = spanEvent.GetEvents();
    APSARA_TEST_EQUAL_FATAL(2U, events.size());
    APSARA_TEST_EQUAL_FATAL(1111111500ULL, events[0].GetTimestampNs());
    APSARA_TEST_EQUAL_FATAL("event1", events[0].GetName().to_string());
    APSARA_TEST_EQUAL_FATAL(1111111800ULL, events[1].GetTimestampNs());
    APSARA_TEST_EQUAL_FATAL("event2", events[1].GetName().to_string());
}

void ManualPBParserUnittest::TestParseSpanEventWithInnerEventTags() {
    // Create PB SpanEvent with inner event that has tags
    models::PipelineEventGroup pbGroup;
    auto* pbSpanEvents = pbGroup.mutable_spans();
    auto* pbSpan = pbSpanEvents->add_events();
    pbSpan->set_traceid("trace_event_tags");
    pbSpan->set_spanid("span_event_tags");
    pbSpan->set_name("span_with_event_tags");
    pbSpan->set_starttime(3333333333ULL);
    pbSpan->set_endtime(4444444444ULL);

    // Add inner event with tags
    auto* innerEvent = pbSpan->add_events();
    innerEvent->set_timestamp(3333333500ULL);
    innerEvent->set_name("exception");
    (*innerEvent->mutable_tags())["exception.type"] = "NullPointerException";
    (*innerEvent->mutable_tags())["exception.message"] = "Object reference not set";

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(1U, eventGroup.GetEvents().size());

    const auto& event = eventGroup.GetEvents()[0];
    APSARA_TEST_TRUE_FATAL(event.Is<SpanEvent>());
    const auto& spanEvent = event.Cast<SpanEvent>();

    // Verify inner event with tags
    const auto& events = spanEvent.GetEvents();
    APSARA_TEST_EQUAL_FATAL(1U, events.size());
    APSARA_TEST_EQUAL_FATAL("exception", events[0].GetName().to_string());
    APSARA_TEST_EQUAL_FATAL(2U, events[0].TagsSize());
    APSARA_TEST_TRUE_FATAL(events[0].HasTag("exception.type"));
    APSARA_TEST_EQUAL_FATAL("NullPointerException", events[0].GetTag("exception.type").to_string());
    APSARA_TEST_TRUE_FATAL(events[0].HasTag("exception.message"));
    APSARA_TEST_EQUAL_FATAL("Object reference not set", events[0].GetTag("exception.message").to_string());
}

void ManualPBParserUnittest::TestParseSpanEventWithLinks() {
    // Create PB SpanEvent with span links
    models::PipelineEventGroup pbGroup;
    auto* pbSpanEvents = pbGroup.mutable_spans();
    auto* pbSpan = pbSpanEvents->add_events();
    pbSpan->set_traceid("trace_with_links");
    pbSpan->set_spanid("span_with_links");
    pbSpan->set_name("span_with_links");
    pbSpan->set_starttime(5555555555ULL);
    pbSpan->set_endtime(6666666666ULL);

    // Add first link
    auto* link1 = pbSpan->add_links();
    link1->set_traceid("linked_trace_1");
    link1->set_spanid("linked_span_1");

    // Add second link
    auto* link2 = pbSpan->add_links();
    link2->set_traceid("linked_trace_2");
    link2->set_spanid("linked_span_2");

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(1U, eventGroup.GetEvents().size());

    const auto& event = eventGroup.GetEvents()[0];
    APSARA_TEST_TRUE_FATAL(event.Is<SpanEvent>());
    const auto& spanEvent = event.Cast<SpanEvent>();

    // Verify span links
    const auto& links = spanEvent.GetLinks();
    APSARA_TEST_EQUAL_FATAL(2U, links.size());
    APSARA_TEST_EQUAL_FATAL("linked_trace_1", links[0].GetTraceId().to_string());
    APSARA_TEST_EQUAL_FATAL("linked_span_1", links[0].GetSpanId().to_string());
    APSARA_TEST_EQUAL_FATAL("linked_trace_2", links[1].GetTraceId().to_string());
    APSARA_TEST_EQUAL_FATAL("linked_span_2", links[1].GetSpanId().to_string());
}

void ManualPBParserUnittest::TestParseSpanEventWithLinkTraceStateAndTags() {
    // Create PB SpanEvent with link that has TraceState and Tags
    models::PipelineEventGroup pbGroup;
    auto* pbSpanEvents = pbGroup.mutable_spans();
    auto* pbSpan = pbSpanEvents->add_events();
    pbSpan->set_traceid("trace_link_attrs");
    pbSpan->set_spanid("span_link_attrs");
    pbSpan->set_name("span_with_link_attrs");
    pbSpan->set_starttime(7777777777ULL);
    pbSpan->set_endtime(8888888888ULL);

    // Add link with TraceState and Tags
    auto* link = pbSpan->add_links();
    link->set_traceid("linked_trace_with_state");
    link->set_spanid("linked_span_with_state");
    link->set_tracestate("key1=value1,key2=value2");
    (*link->mutable_tags())["link.type"] = "follows_from";
    (*link->mutable_tags())["link.source"] = "external_service";

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(1U, eventGroup.GetEvents().size());

    const auto& event = eventGroup.GetEvents()[0];
    APSARA_TEST_TRUE_FATAL(event.Is<SpanEvent>());
    const auto& spanEvent = event.Cast<SpanEvent>();

    // Verify span link with TraceState and Tags
    const auto& links = spanEvent.GetLinks();
    APSARA_TEST_EQUAL_FATAL(1U, links.size());
    APSARA_TEST_EQUAL_FATAL("linked_trace_with_state", links[0].GetTraceId().to_string());
    APSARA_TEST_EQUAL_FATAL("linked_span_with_state", links[0].GetSpanId().to_string());
    APSARA_TEST_EQUAL_FATAL("key1=value1,key2=value2", links[0].GetTraceState().to_string());
    APSARA_TEST_EQUAL_FATAL(2U, links[0].TagsSize());
    APSARA_TEST_TRUE_FATAL(links[0].HasTag("link.type"));
    APSARA_TEST_EQUAL_FATAL("follows_from", links[0].GetTag("link.type").to_string());
    APSARA_TEST_TRUE_FATAL(links[0].HasTag("link.source"));
    APSARA_TEST_EQUAL_FATAL("external_service", links[0].GetTag("link.source").to_string());
}

void ManualPBParserUnittest::TestParseSpanEventWithAllFields() {
    // Create comprehensive PB SpanEvent with all fields including newly added ones
    models::PipelineEventGroup pbGroup;
    auto* pbSpanEvents = pbGroup.mutable_spans();
    auto* pbSpan = pbSpanEvents->add_events();

    // Basic fields
    pbSpan->set_timestamp(9999999999ULL);
    pbSpan->set_traceid("comprehensive_trace");
    pbSpan->set_spanid("comprehensive_span");
    pbSpan->set_tracestate("vendor1=value1");
    pbSpan->set_parentspanid("parent_comprehensive");
    pbSpan->set_name("comprehensive_span");
    pbSpan->set_kind(models::SpanEvent::CLIENT);
    pbSpan->set_starttime(1000000000ULL);
    pbSpan->set_endtime(2000000000ULL);

    // Tags
    (*pbSpan->mutable_tags())["http.method"] = "POST";
    (*pbSpan->mutable_tags())["http.url"] = "/api/test";

    // Inner events with tags
    auto* innerEvent = pbSpan->add_events();
    innerEvent->set_timestamp(1500000000ULL);
    innerEvent->set_name("request_sent");
    (*innerEvent->mutable_tags())["request.size"] = "1024";

    // Links with TraceState and tags
    auto* link = pbSpan->add_links();
    link->set_traceid("related_trace");
    link->set_spanid("related_span");
    link->set_tracestate("vendor2=value2");
    (*link->mutable_tags())["relationship"] = "parent";

    // Status
    pbSpan->set_status(models::SpanEvent::Ok);

    // Scope tags
    (*pbSpan->mutable_scopetags())["otel.scope.name"] = "my.library";
    (*pbSpan->mutable_scopetags())["otel.scope.version"] = "2.0.0";

    // Serialize and parse
    auto data = serializePBEventGroup(pbGroup);
    ManualPBParser parser(data.data(), data.size());
    std::string errMsg;

    PipelineEventGroup eventGroup(mSourceBuffer);
    APSARA_TEST_TRUE_FATAL(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_FATAL(1U, eventGroup.GetEvents().size());

    const auto& event = eventGroup.GetEvents()[0];
    APSARA_TEST_TRUE_FATAL(event.Is<SpanEvent>());
    const auto& spanEvent = event.Cast<SpanEvent>();

    // Verify basic fields
    APSARA_TEST_EQUAL_FATAL("comprehensive_trace", spanEvent.GetTraceId().to_string());
    APSARA_TEST_EQUAL_FATAL("comprehensive_span", spanEvent.GetSpanId().to_string());
    APSARA_TEST_EQUAL_FATAL("vendor1=value1", spanEvent.GetTraceState().to_string());
    APSARA_TEST_EQUAL_FATAL("parent_comprehensive", spanEvent.GetParentSpanId().to_string());
    APSARA_TEST_EQUAL_FATAL("comprehensive_span", spanEvent.GetName().to_string());
    APSARA_TEST_EQUAL_FATAL(SpanEvent::Kind::Client, spanEvent.GetKind());
    APSARA_TEST_EQUAL_FATAL(1000000000ULL, spanEvent.GetStartTimeNs());
    APSARA_TEST_EQUAL_FATAL(2000000000ULL, spanEvent.GetEndTimeNs());

    // Verify tags
    APSARA_TEST_TRUE_FATAL(spanEvent.HasTag("http.method"));
    APSARA_TEST_EQUAL_FATAL("POST", spanEvent.GetTag("http.method").to_string());
    APSARA_TEST_TRUE_FATAL(spanEvent.HasTag("http.url"));
    APSARA_TEST_EQUAL_FATAL("/api/test", spanEvent.GetTag("http.url").to_string());

    // Verify inner events with tags
    const auto& events = spanEvent.GetEvents();
    APSARA_TEST_EQUAL_FATAL(1U, events.size());
    APSARA_TEST_EQUAL_FATAL(1500000000ULL, events[0].GetTimestampNs());
    APSARA_TEST_EQUAL_FATAL("request_sent", events[0].GetName().to_string());
    APSARA_TEST_TRUE_FATAL(events[0].HasTag("request.size"));
    APSARA_TEST_EQUAL_FATAL("1024", events[0].GetTag("request.size").to_string());

    // Verify links with TraceState and tags
    const auto& links = spanEvent.GetLinks();
    APSARA_TEST_EQUAL_FATAL(1U, links.size());
    APSARA_TEST_EQUAL_FATAL("related_trace", links[0].GetTraceId().to_string());
    APSARA_TEST_EQUAL_FATAL("related_span", links[0].GetSpanId().to_string());
    APSARA_TEST_EQUAL_FATAL("vendor2=value2", links[0].GetTraceState().to_string());
    APSARA_TEST_TRUE_FATAL(links[0].HasTag("relationship"));
    APSARA_TEST_EQUAL_FATAL("parent", links[0].GetTag("relationship").to_string());

    // Verify status
    APSARA_TEST_EQUAL_FATAL(SpanEvent::StatusCode::Ok, spanEvent.GetStatus());

    // Verify scope tags
    APSARA_TEST_TRUE_FATAL(spanEvent.HasScopeTag("otel.scope.name"));
    APSARA_TEST_EQUAL_FATAL("my.library", spanEvent.GetScopeTag("otel.scope.name").to_string());
    APSARA_TEST_TRUE_FATAL(spanEvent.HasScopeTag("otel.scope.version"));
    APSARA_TEST_EQUAL_FATAL("2.0.0", spanEvent.GetScopeTag("otel.scope.version").to_string());
}

UNIT_TEST_CASE(ManualPBParserUnittest, TestParseEmptyData)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseNullData)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseInvalidData)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseLogEventWithContent)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseLogEventWithMultipleContents)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseLogEventWithLevel)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseLogEventWithPosition)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMetricEvent)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMetricEventWithTags)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEvent)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEventWithTags)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEventWithScopeTags)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParsePipelineEventGroupWithMetadata)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParsePipelineEventGroupWithTags)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseInvalidWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseIncompletePB)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseUnknownFields)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseReplaceSpanTags)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseLargeVarint)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMultipleLogEvents)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMultipleMetricEvents)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMultipleSpanEvents)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEventWithStatus)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEventWithInnerEvents)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEventWithInnerEventTags)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEventWithLinks)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEventWithLinkTraceStateAndTags)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEventWithAllFields)

} // namespace logtail

UNIT_TEST_MAIN
