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

#ifndef APSARA_UNIT_TEST_MAIN
#define APSARA_UNIT_TEST_MAIN
#endif

#include <memory>
#include <string>

#include "common/memory/SourceBuffer.h"
#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/PipelineEventGroup.h"
#include "models/SpanEvent.h"
#include "plugin/flusher/kafka/TopicFormatParser.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class TopicFormatParserUnittest : public testing::Test {
public:
    void TestBasicDynamicTopic();
    void TestMissingFieldFallback();
    void TestFromTags();
    void TestFromMetricTags();
    void TestFromSpanTags();
    void TestStaticTopic();
    void TestEnvVariableSyntax();
    void TestComplexFormat();
    void TestMissingTagFallback();

protected:
    void SetUp() override { mBuffer = make_shared<SourceBuffer>(); }

    shared_ptr<SourceBuffer> mBuffer;
};

void TopicFormatParserUnittest::TestBasicDynamicTopic() {
    TopicFormatParser parser;
    APSARA_TEST_TRUE(parser.Init("test_%{content.application}"));
    APSARA_TEST_TRUE(parser.IsDynamic());

    PipelineEventGroup group(mBuffer);
    auto logEvent = group.AddLogEvent();
    logEvent->SetContent(StringView("application"), StringView("user_behavior_log"));

    string result;
    PipelineEventPtr eventPtr(logEvent->Copy(), true, nullptr);
    bool success = parser.FormatTopic(eventPtr, result, group.GetTags());
    APSARA_TEST_TRUE(success);
    APSARA_TEST_EQUAL("test_user_behavior_log", result);
}

void TopicFormatParserUnittest::TestMissingFieldFallback() {
    TopicFormatParser parser;
    APSARA_TEST_TRUE(parser.Init("test_%{content.application}"));

    PipelineEventGroup group(mBuffer);
    auto logEvent = group.AddLogEvent();
    logEvent->SetContent(StringView("message"), StringView("test message"));

    string result;
    PipelineEventPtr eventPtr(logEvent->Copy(), true, nullptr);
    bool success = parser.FormatTopic(eventPtr, result, group.GetTags());
    APSARA_TEST_FALSE(success);
}

void TopicFormatParserUnittest::TestFromTags() {
    TopicFormatParser parser;
    APSARA_TEST_TRUE(parser.Init("logs_%{tag.namespace}"));

    PipelineEventGroup group(mBuffer);
    auto logEvent = group.AddLogEvent();
    group.SetTag(StringView("namespace"), StringView("nginx_access_log"));
    logEvent->SetContent(StringView("message"), StringView("test message"));

    string result;
    PipelineEventPtr eventPtr(logEvent->Copy(), true, nullptr);
    bool success = parser.FormatTopic(eventPtr, result, group.GetTags());
    APSARA_TEST_TRUE(success);
    APSARA_TEST_EQUAL("logs_nginx_access_log", result);
}

void TopicFormatParserUnittest::TestFromMetricTags() {
    TopicFormatParser parser;
    APSARA_TEST_TRUE(parser.Init("metrics_%{tag.service}"));

    PipelineEventGroup group(mBuffer);
    auto metricEvent = group.AddMetricEvent();
    metricEvent->SetName(std::string("cpu_usage"));
    metricEvent->SetValue<UntypedSingleValue>(0.75);
    group.SetTag(StringView("service"), StringView("web_server"));

    string result;
    PipelineEventPtr eventPtr(metricEvent->Copy(), true, nullptr);
    bool success = parser.FormatTopic(eventPtr, result, group.GetTags());
    APSARA_TEST_TRUE(success);
    APSARA_TEST_EQUAL("metrics_web_server", result);
}

void TopicFormatParserUnittest::TestFromSpanTags() {
    TopicFormatParser parser;
    APSARA_TEST_TRUE(parser.Init("spans_%{tag.operation}"));

    PipelineEventGroup group(mBuffer);
    auto spanEvent = group.AddSpanEvent();
    spanEvent->SetName(std::string("http_request"));
    spanEvent->SetStartTimeNs(1234567890);
    spanEvent->SetEndTimeNs(1234567900);
    group.SetTag(StringView("operation"), StringView("GET_users"));

    string result;
    PipelineEventPtr eventPtr(spanEvent->Copy(), true, nullptr);
    bool success = parser.FormatTopic(eventPtr, result, group.GetTags());
    APSARA_TEST_TRUE(success);
    APSARA_TEST_EQUAL("spans_GET_users", result);
}

void TopicFormatParserUnittest::TestStaticTopic() {
    TopicFormatParser parser;
    APSARA_TEST_TRUE(parser.Init("static_topic"));
    APSARA_TEST_FALSE(parser.IsDynamic());

    PipelineEventGroup group(mBuffer);
    auto logEvent = group.AddLogEvent();
    logEvent->SetContent(StringView("message"), StringView("test message"));

    string result;
    PipelineEventPtr eventPtr(logEvent->Copy(), true, nullptr);
    bool success = parser.FormatTopic(eventPtr, result, group.GetTags());
    APSARA_TEST_TRUE(success);
    APSARA_TEST_EQUAL("static_topic", result);
}

void TopicFormatParserUnittest::TestEnvVariableSyntax() {
    TopicFormatParser parser;
    APSARA_TEST_TRUE(parser.Init("app_${ENVIRONMENT}"));

    PipelineEventGroup group(mBuffer);
    auto logEvent = group.AddLogEvent();
    group.SetTag(StringView("ENVIRONMENT"), StringView("production"));
    logEvent->SetContent(StringView("message"), StringView("test message"));

    string result;
    PipelineEventPtr eventPtr(logEvent->Copy(), true, nullptr);
    bool success = parser.FormatTopic(eventPtr, result, group.GetTags());
    APSARA_TEST_TRUE(success);
    APSARA_TEST_EQUAL("app_production", result);
}

void TopicFormatParserUnittest::TestComplexFormat() {
    TopicFormatParser parser;
    APSARA_TEST_TRUE(parser.Init("service_%{tag.env}_%{content.app}_logs"));

    PipelineEventGroup group(mBuffer);
    auto logEvent = group.AddLogEvent();
    group.SetTag(StringView("env"), StringView("prod"));
    logEvent->SetContent(StringView("app"), StringView("payment"));
    logEvent->SetContent(StringView("message"), StringView("test message"));

    string result;
    PipelineEventPtr eventPtr(logEvent->Copy(), true, nullptr);
    bool success = parser.FormatTopic(eventPtr, result, group.GetTags());
    APSARA_TEST_TRUE(success);
    APSARA_TEST_EQUAL("service_prod_payment_logs", result);
}

void TopicFormatParserUnittest::TestMissingTagFallback() {
    TopicFormatParser parser;
    APSARA_TEST_TRUE(parser.Init("logs_%{tag.namespace}"));

    PipelineEventGroup group(mBuffer);
    auto logEvent = group.AddLogEvent();
    logEvent->SetContent(StringView("message"), StringView("test message"));

    string result;
    PipelineEventPtr eventPtr(logEvent->Copy(), true, nullptr);
    bool success = parser.FormatTopic(eventPtr, result, group.GetTags());
    APSARA_TEST_FALSE(success);
}

UNIT_TEST_CASE(TopicFormatParserUnittest, TestBasicDynamicTopic)
UNIT_TEST_CASE(TopicFormatParserUnittest, TestMissingFieldFallback)
UNIT_TEST_CASE(TopicFormatParserUnittest, TestFromTags)
UNIT_TEST_CASE(TopicFormatParserUnittest, TestFromMetricTags)
UNIT_TEST_CASE(TopicFormatParserUnittest, TestFromSpanTags)
UNIT_TEST_CASE(TopicFormatParserUnittest, TestStaticTopic)
UNIT_TEST_CASE(TopicFormatParserUnittest, TestEnvVariableSyntax)
UNIT_TEST_CASE(TopicFormatParserUnittest, TestComplexFormat)
UNIT_TEST_CASE(TopicFormatParserUnittest, TestMissingTagFallback)

} // namespace logtail

UNIT_TEST_MAIN
