/*
 * Copyright 2024 iLogtail Authors
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

#include "LogEvent.h"
#include "MetricEvent.h"
#include "StringTools.h"
#include "common/JsonUtil.h"
#include "processor/inner/ProcessorPromParseMetricNative.h"
#include "prometheus/Constants.h"
#include "prometheus/labels/TextParser.h"
#include "prometheus/schedulers/ScrapeScheduler.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
class ProcessorParsePrometheusMetricUnittest : public testing::Test {
public:
    void SetUp() override { mContext.SetConfigName("project##config_0"); }

    void TestInit();
    void TestProcess();

    PipelineContext mContext;
};

void ProcessorParsePrometheusMetricUnittest::TestInit() {
    Json::Value config;
    ProcessorPromParseMetricNative processor;
    processor.SetContext(mContext);

    // success config
    string configStr;
    string errorMsg;
    configStr = R"JSON(
        {
        }
    )JSON";

    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    APSARA_TEST_TRUE(processor.Init(config));
}

void ProcessorParsePrometheusMetricUnittest::TestProcess() {
    // make config
    Json::Value config;

    ProcessorPromParseMetricNative processor;
    processor.SetContext(mContext);

    string configStr;
    string errorMsg;
    configStr = configStr + R"(
        {
        }
    )";

    // init
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    APSARA_TEST_TRUE(processor.Init(config));

    // make events
    auto parser = TextParser();
    auto splitByLines = [](const std::string& content, time_t timestamp) {
        PipelineEventGroup eGroup(std::make_shared<SourceBuffer>());

        for (const auto& line : SplitString(content, "\r\n")) {
            auto newLine = TrimString(line);
            if (newLine.empty() || newLine[0] == '#') {
                continue;
            }
            auto* MetricEvent = eGroup.AddLogEvent();
            MetricEvent->SetContent(prometheus::PROMETHEUS, newLine);
            MetricEvent->SetTimestamp(timestamp);
        }

        return eGroup;
    };
    auto eventGroup = splitByLines(R"""(
                # begin

                test_metric1{k1="v1", k2="v2"} 1.0
                test_metric2{k1="v1", k2="v2"} 2.0 1234567890
                test_metric3{k1="v1",k2="v2"} 9.9410452992e+10
                test_metric4{k1="v1",k2="v2"} 9.9410452992e+10 1715829785083
                test_metric5{k1="v1", k2="v2" } 9.9410452992e+10 1715829785083
                test_metric6{k1="v1",k2="v2",} 9.9410452992e+10 1715829785083
                test_metric7{k1="v1",k3="2", } 9.9410452992e+10 1715829785083  
                test_metric8{k1="v1", k3="v2", } 9.9410452992e+10 1715829785083

                # end
                    )""",
                                   0);

    // run function
    APSARA_TEST_EQUAL((size_t)8, eventGroup.GetEvents().size());
    processor.Process(eventGroup);

    // judge result
    APSARA_TEST_EQUAL((size_t)8, eventGroup.GetEvents().size());
    APSARA_TEST_EQUAL("test_metric1", eventGroup.GetEvents().at(0).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric2", eventGroup.GetEvents().at(1).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric3", eventGroup.GetEvents().at(2).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric4", eventGroup.GetEvents().at(3).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric5", eventGroup.GetEvents().at(4).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric6", eventGroup.GetEvents().at(5).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric7", eventGroup.GetEvents().at(6).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric8", eventGroup.GetEvents().at(7).Cast<MetricEvent>().GetName());
}

UNIT_TEST_CASE(ProcessorParsePrometheusMetricUnittest, TestInit)
UNIT_TEST_CASE(ProcessorParsePrometheusMetricUnittest, TestProcess)

} // namespace logtail

UNIT_TEST_MAIN