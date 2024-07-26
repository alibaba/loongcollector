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

#include <json/json.h>

#include <boost/regex.hpp>
#include <string>

#include "common/JsonUtil.h"
#include "prometheus/Relabel.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class ActionConverterUnittest : public testing::Test {
public:
    void TestStringToAction();
    void TestActionToString();
};

class RelabelConfigUnittest : public testing::Test {
public:
    void TestRelabelConfig();
    void TestProcess();
};


void ActionConverterUnittest::TestStringToAction() {
    // replace
    std::string actionString = "replace";
    Action expectedAction = Action::REPLACE;

    // Act
    Action result = StringToAction(actionString);

    // Assert
    APSARA_TEST_EQUAL(result, expectedAction);
}

void ActionConverterUnittest::TestActionToString() {
    // replace
    Action action = Action::REPLACE;
    std::string expectedString = "replace";

    // Act
    std::string result = ActionToString(action);

    // Assert
    APSARA_TEST_EQUAL(result, expectedString);
}

void RelabelConfigUnittest::TestRelabelConfig() {
    Json::Value configJson;
    string configStr, errorMsg;

    configStr = R"(
        {
                "action": "keep",
                "regex": "node-exporter",
                "replacement": "$1",
                "separator": ";",
                "source_labels": [
                    "__meta_kubernetes_pod_label_app"
                ],
                "target_label": "__address__",
                "modulus": 222
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));

    RelabelConfig config = RelabelConfig(configJson);

    APSARA_TEST_EQUAL(true, config.Validate());

    APSARA_TEST_EQUAL(Action::KEEP, config.mAction);
    // APSARA_TEST_EQUAL("node-exporter", config.regex.get_data());
    APSARA_TEST_EQUAL("$1", config.mReplacement);
    APSARA_TEST_EQUAL(";", config.mSeparator);
    APSARA_TEST_EQUAL((size_t)1, config.mSourceLabels.size());
    APSARA_TEST_EQUAL("__meta_kubernetes_pod_label_app", config.mSourceLabels[0]);
    APSARA_TEST_EQUAL("__address__", config.mTargetLabel);
    APSARA_TEST_EQUAL((uint64_t)222, config.mModulus);
}

void RelabelConfigUnittest::TestProcess() {
    Json::Value configJson;
    string configStr, errorMsg;
    Labels labels;
    labels.Push(Label{"__meta_kubernetes_pod_ip", "172.17.0.3"});
    labels.Push(Label{"__meta_kubernetes_pod_label_app", "node-exporter"});
    vector<RelabelConfig> cfgs;

    // single relabel replace
    configStr = configStr + R"(
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
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    RelabelConfig config = RelabelConfig(configJson);
    cfgs.push_back(config);

    Labels result;
    relabel::Process(labels, cfgs, result);

    APSARA_TEST_EQUAL((size_t)3, result.Size());
    APSARA_TEST_EQUAL("172.17.0.3:9100", result.Get("__address__"));
    APSARA_TEST_EQUAL("node-exporter", result.Get("__meta_kubernetes_pod_label_app"));
    APSARA_TEST_EQUAL("172.17.0.3", result.Get("__meta_kubernetes_pod_ip"));

    // single relabel keep
    configStr = R"(
        {
                "action": "keep",
                "regex": "172.*",
                "separator": ";",
                "source_labels": [
                    "__meta_kubernetes_pod_ip"
                ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config = RelabelConfig(configJson);
    cfgs.clear();
    cfgs.push_back(config);
    relabel::Process(labels, cfgs, result);

    APSARA_TEST_EQUAL((size_t)2, result.Size());
    APSARA_TEST_EQUAL("172.17.0.3", result.Get("__meta_kubernetes_pod_ip"));

    // single relabel drop
    configStr = R"(
        {
                "action": "drop",
                "regex": "172.*",
                "source_labels": [
                    "__meta_kubernetes_pod_ip"
                ]
        }
    )";

    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config = RelabelConfig(configJson);
    cfgs.clear();
    cfgs.push_back(config);
    relabel::Process(labels, cfgs, result);
    APSARA_TEST_EQUAL((size_t)0, result.Size());
    APSARA_TEST_EQUAL("", result.Get("__meta_kubernetes_pod_label_app"));

    // relabel dropequal
    configStr = R"(
        {
                "action": "dropequal",
                "regex": "172.*",
                "source_labels": [
                    "__meta_kubernetes_pod_ip"
                ],
                "target_label": "pod_ip"
        }
    )";
    Labels dropEqualLabels;
    dropEqualLabels.Push(Label{"__meta_kubernetes_pod_ip", "172.17.0.3"});
    dropEqualLabels.Push(Label{"pod_ip", "172.17.0.3"});
    dropEqualLabels.Push(Label{"__meta_kubernetes_pod_label_app", "node-exporter"});

    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config = RelabelConfig(configJson);
    cfgs.clear();
    cfgs.push_back(config);
    relabel::Process(dropEqualLabels, cfgs, result);
    APSARA_TEST_EQUAL((size_t)0, result.Size());
    APSARA_TEST_EQUAL("", result.Get("__meta_kubernetes_pod_label_app"));

    // relabel keepequal
    configStr = R"(
        {
                "action": "keepequal",
                "regex": "172.*",
                "source_labels": [
                    "__meta_kubernetes_pod_ip"
                ],
                "target_label": "pod_ip"
        }
    )";
    Labels keepEqualLabels;
    keepEqualLabels.Push(Label{"__meta_kubernetes_pod_ip", "172.17.0.3"});
    keepEqualLabels.Push(Label{"pod_ip", "172.17.0.3"});
    keepEqualLabels.Push(Label{"__meta_kubernetes_pod_label_app", "node-exporter"});

    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config = RelabelConfig(configJson);
    cfgs.clear();
    cfgs.push_back(config);
    relabel::Process(keepEqualLabels, cfgs, result);
    APSARA_TEST_EQUAL((size_t)3, result.Size());
    APSARA_TEST_EQUAL("172.17.0.3", result.Get("__meta_kubernetes_pod_ip"));

    // relabel lowercase
    configStr = R"(
        {
                "action": "lowercase",
                "regex": "172.*",
                "source_labels": [
                    "__meta_kubernetes_pod_label_app"
                ],
                "target_label": "__meta_kubernetes_pod_label_app"
        }
    )";
    Labels lowercaseLabels;
    lowercaseLabels.Push(Label{"__meta_kubernetes_pod_ip", "172.17.0.3"});
    lowercaseLabels.Push(Label{"pod_ip", "172.17.0.3"});
    lowercaseLabels.Push(Label{"__meta_kubernetes_pod_label_app", "node-Exporter"});

    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config = RelabelConfig(configJson);
    cfgs.clear();
    cfgs.push_back(config);
    relabel::Process(lowercaseLabels, cfgs, result);
    APSARA_TEST_EQUAL((size_t)3, result.Size());
    APSARA_TEST_EQUAL("node-exporter", result.Get("__meta_kubernetes_pod_label_app"));

    // relabel uppercase
    configStr = R"(
        {
                "action": "uppercase",
                "regex": "172.*",
                "source_labels": [
                    "__meta_kubernetes_pod_label_app"
                ],
                "target_label": "__meta_kubernetes_pod_label_app"
        }
    )";
    Labels uppercaseLabels;
    uppercaseLabels.Push(Label{"__meta_kubernetes_pod_ip", "172.17.0.3"});
    uppercaseLabels.Push(Label{"pod_ip", "172.17.0.3"});
    uppercaseLabels.Push(Label{"__meta_kubernetes_pod_label_app", "node-Exporter"});

    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config = RelabelConfig(configJson);
    cfgs.clear();
    cfgs.push_back(config);
    relabel::Process(uppercaseLabels, cfgs, result);
    APSARA_TEST_EQUAL((size_t)3, result.Size());
    APSARA_TEST_EQUAL("NODE-EXPORTER", result.Get("__meta_kubernetes_pod_label_app"));

    // relabel hashmod
    configStr = R"(
        {
                "action": "hashmod",
                "regex": "172.*",
                "source_labels": [
                    "__meta_kubernetes_pod_label_app"
                ],
                "target_label": "hash_val",
                "modulus": 255
        }
    )";
    Labels hashmodLabels;
    hashmodLabels.Push(Label{"__meta_kubernetes_pod_ip", "172.17.0.3"});
    hashmodLabels.Push(Label{"pod_ip", "172.17.0.3"});
    hashmodLabels.Push(Label{"__meta_kubernetes_pod_label_app", "node-Exporter"});

    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config = RelabelConfig(configJson);
    cfgs.clear();
    cfgs.push_back(config);
    relabel::Process(hashmodLabels, cfgs, result);
    APSARA_TEST_EQUAL((size_t)4, result.Size());
    APSARA_TEST_TRUE(!result.Get("hash_val").empty());

    configStr.clear();
    // single relabel labelmap
    configStr = configStr + R"(
        {
                "action": "labelmap",
                "regex": "__meta_kubernetes_pod_label_(.+)"
        + ")\"," + R"("replacement": "k8s_$1"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config = RelabelConfig(configJson);
    cfgs.clear();
    cfgs.push_back(config);
    relabel::Process(labels, cfgs, result);
    APSARA_TEST_EQUAL((size_t)3, result.Size());
    APSARA_TEST_EQUAL("node-exporter", result.Get("k8s_app"));

    // relabel labeldrop
    configStr = R"(
        {
                "action": "labeldrop",
                "regex": "__meta.*",
        }
    )";
    Labels labelDropLabels;
    labelDropLabels.Push(Label{"__meta_kubernetes_pod_ip", "172.17.0.3"});
    labelDropLabels.Push(Label{"pod_ip", "172.17.0.3"});
    labelDropLabels.Push(Label{"__meta_kubernetes_pod_label_app", "node-Exporter"});

    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config = RelabelConfig(configJson);
    cfgs.clear();
    cfgs.push_back(config);
    relabel::Process(labelDropLabels, cfgs, result);
    APSARA_TEST_EQUAL((size_t)1, result.Size());
    APSARA_TEST_EQUAL("172.17.0.3", result.Get("pod_ip"));

    // relabel labelkeep
    configStr = R"(
        {
                "action": "labelkeep",
                "regex": "__meta.*",
        }
    )";
    Labels labelKeepLabels;
    labelKeepLabels.Push(Label{"__meta_kubernetes_pod_ip", "172.17.0.3"});
    labelKeepLabels.Push(Label{"pod_ip", "172.17.0.3"});
    labelKeepLabels.Push(Label{"__meta_kubernetes_pod_label_app", "node-exporter"});

    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    config = RelabelConfig(configJson);
    cfgs.clear();
    cfgs.push_back(config);
    relabel::Process(labelKeepLabels, cfgs, result);
    APSARA_TEST_EQUAL((size_t)2, result.Size());
    APSARA_TEST_EQUAL("172.17.0.3", result.Get("__meta_kubernetes_pod_ip"));
    APSARA_TEST_EQUAL("node-exporter", result.Get("__meta_kubernetes_pod_label_app"));


    // multi relabel
    string configStr1, configStr2;
    configStr1 = configStr1 + R"(
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
    )";
    configStr2 = R"(
        {
                "action": "drop",
                "regex": "172.*",
                "separator": ";",
                "source_labels": [
                    "__address__"
                ]
        }
    )";

    APSARA_TEST_TRUE(ParseJsonTable(configStr1, configJson, errorMsg));
    config = RelabelConfig(configJson);
    cfgs.clear();
    cfgs.push_back(config);
    APSARA_TEST_TRUE(ParseJsonTable(configStr2, configJson, errorMsg));
    config = RelabelConfig(configJson);
    cfgs.push_back(config);

    relabel::Process(labels, cfgs, result);
    APSARA_TEST_EQUAL((size_t)0, result.Size());
    APSARA_TEST_EQUAL("", result.Get("__address__"));
}

UNIT_TEST_CASE(ActionConverterUnittest, TestStringToAction)
UNIT_TEST_CASE(ActionConverterUnittest, TestActionToString)

UNIT_TEST_CASE(RelabelConfigUnittest, TestRelabelConfig)
UNIT_TEST_CASE(RelabelConfigUnittest, TestProcess)

} // namespace logtail

UNIT_TEST_MAIN
