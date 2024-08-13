/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string>

#include "MetricEvent.h"
#include "models/PipelineEventGroup.h"
#include "prometheus/labels/TextParser.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

bool isDoubleEqual(double a, double b) {
    return fabs(a - b) < 0.000001;
}

class TextParserUnittest : public testing::Test {
public:
    void TestMetricEvent() const;
    void TestParseMultipleLines() const;
    void TestSampleRegex() const;
    void TestParseMetricWithTagsAndTimestamp() const;
    void TestParseMetricWithManyTags() const;
};

void TextParserUnittest::TestMetricEvent() const {
    const auto& srcBuf = make_shared<SourceBuffer>();
    auto eGroup = PipelineEventGroup(srcBuf);
    auto event = eGroup.AddMetricEvent();
    event->SetName("test_metric");
    event->SetValue(MetricValue(UntypedSingleValue{1.0}));
    event->SetTimestamp(1234567890);
    event->SetTag(StringView("test_key"), StringView("test_value"));

    const auto& events = eGroup.GetEvents();
    APSARA_TEST_EQUAL(1UL, events.size());
    const auto& firstEvent = &events.front();
    const auto& firstMetric = firstEvent->Get<MetricEvent>();
    APSARA_TEST_STREQ("test_metric", firstMetric->GetName().data());
    const auto& metricValue = firstMetric->GetValue<UntypedSingleValue>();
    APSARA_TEST_EQUAL(1.0, metricValue->mValue);
    APSARA_TEST_EQUAL(1234567890, firstMetric->GetTimestamp());
    APSARA_TEST_STREQ("test_value", firstMetric->GetTag(logtail::StringView("test_key")).data());
}
UNIT_TEST_CASE(TextParserUnittest, TestMetricEvent)

void TextParserUnittest::TestParseMultipleLines() const {
    auto parser = TextParser();
    const auto eGroup = parser.Parse(R"""(
# begin

test_metric1{k1="v1", k2="v 1.0
  test_metric2{k1="v1", k2="v2"} 2.0 1234567890
test_metric3{k1="v1",k2="v2"} 9.9410452992e+10
  test_metric4{k1="v1",k2="v2"} 9.9410452992e+10 1715829785083
  test_metric5{k1="v1", k2="v2" } 9.9410452992e+10 1715829785083
test_metric6{k1="v1",k2="v2",} 9.9410452992e+10 1715829785083
test_metric7{k1="v1",k2="v2", } 9.9410452992e+10 1715829785083  
test_metric8{k1="v1", k2="v2", } 9.9410452992e+10 1715829785083

# end
    )""");
    const auto& events = &eGroup.GetEvents();
    APSARA_TEST_EQUAL(7UL, events->size());
}
UNIT_TEST_CASE(TextParserUnittest, TestParseMultipleLines)

void TextParserUnittest::TestParseMetricWithTagsAndTimestamp() const {
    auto parser = TextParser();
    const auto eGroup = parser.Parse(R"""(
    test_metric{k1="v1", k2="v2"} 9.9410452992e+10 1715829785083
    test_metric2{k1="v1", k2="v2"} 2.0 1715829785083
    test_metric3{k1="v1",k2="v2"} 4.2 92233720368547758080000
    )""");

    // test_metric
    const auto& events = &eGroup.GetEvents();
    const auto& event = events->front();
    const auto& metric = event.Get<MetricEvent>();
    APSARA_TEST_STREQ("test_metric", metric->GetName().data());
    APSARA_TEST_EQUAL(1715829785, metric->GetTimestamp());
    APSARA_TEST_TRUE(isDoubleEqual(9.9410452992e+10, metric->GetValue<UntypedSingleValue>()->mValue));
    APSARA_TEST_STREQ("v1", metric->GetTag("k1").data());
    APSARA_TEST_STREQ("v2", metric->GetTag("k2").data());

    // test_metric2
    auto& event2 = events->at(1);
    const auto& metric2 = event2.Get<MetricEvent>();
    APSARA_TEST_STREQ("test_metric2", metric2->GetName().data());
    APSARA_TEST_EQUAL(1715829785, metric2->GetTimestamp());
    APSARA_TEST_TRUE(isDoubleEqual(2.0, metric2->GetValue<UntypedSingleValue>()->mValue));
    APSARA_TEST_STREQ("v1", metric2->GetTag("k1").data());
    APSARA_TEST_STREQ("v2", metric2->GetTag("k2").data());

    // test_metric3 is not generated because of timestamp overflow
    APSARA_TEST_EQUAL(2UL, events->size());
}
UNIT_TEST_CASE(TextParserUnittest, TestParseMetricWithTagsAndTimestamp)

void TextParserUnittest::TestParseMetricWithManyTags() const {
    auto parser = TextParser();
    const auto eGroup = parser.Parse(
        R"""(container_blkio_device_usage_total{container="",device="/dev/nvme0n1",id="/",image="",major="259",minor="0",name="",namespace="",operation="Async",pod=""} 9.9410452992e+10 1715829785083)""",
        1715829785083,
        "test_job",
        "test_instance");
    const auto& events = &eGroup.GetEvents();
    APSARA_TEST_EQUAL(1UL, events->size());
    const auto& event = events->front();
    const auto& metric = event.Get<MetricEvent>();
    APSARA_TEST_STREQ("container_blkio_device_usage_total", metric->GetName().data());
    APSARA_TEST_EQUAL(1715829785, metric->GetTimestamp());
    APSARA_TEST_TRUE(isDoubleEqual(9.9410452992e+10, metric->GetValue<UntypedSingleValue>()->mValue));

    // TODO: assert number of tags
    APSARA_TEST_STREQ("", metric->GetTag("container").data());
    APSARA_TEST_STREQ("/dev/nvme0n1", metric->GetTag("device").data());
    APSARA_TEST_STREQ("/", metric->GetTag("id").data());
    APSARA_TEST_STREQ("", metric->GetTag("image").data());
    APSARA_TEST_STREQ("259", metric->GetTag("major").data());
    APSARA_TEST_STREQ("0", metric->GetTag("minor").data());
    APSARA_TEST_STREQ("", metric->GetTag("name").data());
    APSARA_TEST_STREQ("", metric->GetTag("namespace").data());
    APSARA_TEST_STREQ("Async", metric->GetTag("operation").data());
    APSARA_TEST_STREQ("", metric->GetTag("pod").data());
}
UNIT_TEST_CASE(TextParserUnittest, TestParseMetricWithManyTags)

} // namespace logtail

UNIT_TEST_MAIN
