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

#include "MetricEvent.h"
#include "TextParser.h"
#include "common/JsonUtil.h"
#include "processor/inner/ProcessorPromRelabelMetricNative.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
class ProcessorPromRelabelMetricNativeUnittest : public testing::Test {
public:
    void SetUp() override { mContext.SetConfigName("project##config_0"); }

    void TestInit();
    void TestProcess();

    PipelineContext mContext;
};

void ProcessorPromRelabelMetricNativeUnittest::TestInit() {
    Json::Value config;
    ProcessorPromRelabelMetricNative processor;
    processor.SetContext(mContext);

    // success config
    string configStr, errorMsg;
    configStr = R"JSON(
        {
            "metric_relabel_configs": [
                {
                    "action": "keep",
                    "regex": "node-exporter",
                    "replacement": "$1",
                    "separator": ";",
                    "source_labels": [
                        "__meta_kubernetes_pod_label_app"
                    ]
                },
                {
                    "action": "replace",
                    "regex": "(.*)",
                    "replacement": "${1}:9100",
                    "separator": ";",
                    "source_labels": [
                        "__meta_kubernetes_pod_ip"
                    ],
                    "target_label": "__address__"
                }
            ]
        }
    )JSON";

    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    APSARA_TEST_TRUE(processor.Init(config));
}

void ProcessorPromRelabelMetricNativeUnittest::TestProcess() {
    // make config
    Json::Value config;

    ProcessorPromRelabelMetricNative processor;
    processor.SetContext(mContext);

    string configStr, errorMsg;
    configStr = configStr + R"(
        {
            "metric_relabel_configs": [
                {
                    "action": "drop",
                    "regex": "v.*",
                    "replacement": "$1",
                    "separator": ";",
                    "source_labels": [
                        "k3"
                    ]
                },
                {
                    "action": "replace",
                    "regex": "(.*)"
        + ")\",\n" +
        R"(
                    "replacement": "${1}:9100",
                    "separator": ";",
                    "source_labels": [
                        "__meta_kubernetes_pod_ip"
                    ],
                    "target_label": "__address__"
                }
            ]
        }
    )";

    // init
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    APSARA_TEST_TRUE(processor.Init(config));

    // make events
    auto parser = TextParser();
    auto eventGroup = parser.Parse(R"""(
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
    )""");

    // run function
    std::string pluginId = "testID";
    APSARA_TEST_EQUAL((size_t)8, eventGroup.GetEvents().size());
    processor.Process(eventGroup);

    // judge result
    APSARA_TEST_EQUAL((size_t)7, eventGroup.GetEvents().size());
    APSARA_TEST_EQUAL("test_metric1", eventGroup.GetEvents().at(0).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric2", eventGroup.GetEvents().at(1).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric3", eventGroup.GetEvents().at(2).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric4", eventGroup.GetEvents().at(3).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric5", eventGroup.GetEvents().at(4).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric6", eventGroup.GetEvents().at(5).Cast<MetricEvent>().GetName());
    APSARA_TEST_EQUAL("test_metric7", eventGroup.GetEvents().at(6).Cast<MetricEvent>().GetName());
    // test_metric8 is dropped by relabel config
}

UNIT_TEST_CASE(ProcessorPromRelabelMetricNativeUnittest, TestInit)
UNIT_TEST_CASE(ProcessorPromRelabelMetricNativeUnittest, TestProcess)

} // namespace logtail

UNIT_TEST_MAIN