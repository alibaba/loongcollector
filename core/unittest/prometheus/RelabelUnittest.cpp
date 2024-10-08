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
#include "prometheus/labels/Relabel.h"
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
    void TestReplace();
    void TestKeep();
    void TestDrop();
    void TestDropEqual();
    void TestHashMod();
    void TestLabelDrop();
    void TestLabelKeep();
    void TestLabelMap();
    void TestKeepEqual();
    void TestLowerCase();
    void TestUpperCase();
    void TestMultiRelabel();
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
    string configStr;
    string errorMsg;

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

    RelabelConfig config = RelabelConfig();

    APSARA_TEST_EQUAL(true, config.Init(configJson));

    APSARA_TEST_EQUAL(Action::KEEP, config.mAction);
    // APSARA_TEST_EQUAL("node-exporter", config.regex.get_data());
    APSARA_TEST_EQUAL("$1", config.mReplacement);
    APSARA_TEST_EQUAL(";", config.mSeparator);
    APSARA_TEST_EQUAL((size_t)1, config.mSourceLabels.size());
    APSARA_TEST_EQUAL("__meta_kubernetes_pod_label_app", config.mSourceLabels[0]);
    APSARA_TEST_EQUAL("__address__", config.mTargetLabel);
    APSARA_TEST_EQUAL((uint64_t)222, config.mModulus);
}


void RelabelConfigUnittest::TestReplace() {
    Json::Value configJson;
    string configStr;
    string errorMsg;
    Labels labels;
    labels.Set("__meta_kubernetes_pod_ip", "172.17.0.3");
    labels.Set("__meta_kubernetes_pod_label_app", "node-exporter");

    // single relabel replace
    configStr = configStr + R"JSON(
        [{
                "action": "replace",
                "regex": "(.*)",
                "replacement": "${1}:9100",
                "separator": ";",
                "source_labels": [
                    "__meta_kubernetes_pod_ip"
                ],
                "target_label": "__address__"
        }]
    )JSON";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    RelabelConfigList configList = RelabelConfigList();
    APSARA_TEST_TRUE(configList.Init(configJson));

    Labels result = labels;
    configList.Process(result);

    APSARA_TEST_EQUAL((size_t)3, result.Size());
    APSARA_TEST_EQUAL("172.17.0.3:9100", result.Get("__address__"));
    APSARA_TEST_EQUAL("node-exporter", result.Get("__meta_kubernetes_pod_label_app"));
    APSARA_TEST_EQUAL("172.17.0.3", result.Get("__meta_kubernetes_pod_ip"));
}
void RelabelConfigUnittest::TestKeep() {
    Json::Value configJson;
    string configStr;
    string errorMsg;
    RelabelConfigList configList;
    Labels labels;
    labels.Set("__meta_kubernetes_pod_ip", "172.17.0.3");
    labels.Set("__meta_kubernetes_pod_label_app", "node-exporter");
    // single relabel keep
    configStr = R"(
        [{
                "action": "keep",
                "regex": "172.*",
                "separator": ";",
                "source_labels": [
                    "__meta_kubernetes_pod_ip"
                ]
        }]
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configList = RelabelConfigList();
    APSARA_TEST_TRUE(configList.Init(configJson));
    auto result = labels;
    configList.Process(result);
    APSARA_TEST_EQUAL((size_t)2, result.Size());
    APSARA_TEST_EQUAL("172.17.0.3", result.Get("__meta_kubernetes_pod_ip"));
}

void RelabelConfigUnittest::TestDrop() {
    Json::Value configJson;
    string configStr;
    string errorMsg;
    RelabelConfigList configList;
    Labels labels;
    labels.Set("__meta_kubernetes_pod_ip", "172.17.0.3");
    labels.Set("__meta_kubernetes_pod_label_app", "node-exporter");
    // single relabel drop
    configStr = R"(
        [{
                "action": "drop",
                "regex": "172.*",
                "source_labels": [
                    "__meta_kubernetes_pod_ip"
                ]
        }]
    )";

    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configList = RelabelConfigList();
    APSARA_TEST_TRUE(configList.Init(configJson));
    auto result = labels;

    APSARA_TEST_FALSE(configList.Process(result));
}

void RelabelConfigUnittest::TestDropEqual() {
    Json::Value configJson;
    string configStr;
    string errorMsg;
    RelabelConfigList configList;
    // relabel dropequal
    configStr = R"(
        [{
                "action": "dropequal",
                "regex": "172.*",
                "source_labels": [
                    "__meta_kubernetes_pod_ip"
                ],
                "target_label": "pod_ip"
        }]
    )";
    Labels dropEqualLabels;
    dropEqualLabels.Set("__meta_kubernetes_pod_ip", "172.17.0.3");
    dropEqualLabels.Set("pod_ip", "172.17.0.3");
    dropEqualLabels.Set("__meta_kubernetes_pod_label_app", "node-exporter");

    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configList = RelabelConfigList();
    APSARA_TEST_TRUE(configList.Init(configJson));
    auto result = dropEqualLabels;
    APSARA_TEST_FALSE(configList.Process(result));
}

void RelabelConfigUnittest::TestKeepEqual() {
    Json::Value configJson;
    string configStr;
    string errorMsg;
    RelabelConfigList configList;
    // relabel keepequal
    configStr = R"(
        [{
                "action": "keepequal",
                "regex": "172.*",
                "source_labels": [
                    "__meta_kubernetes_pod_ip"
                ],
                "target_label": "pod_ip"
        }]
    )";
    Labels keepEqualLabels;
    keepEqualLabels.Set("__meta_kubernetes_pod_ip", "172.17.0.3");
    keepEqualLabels.Set("pod_ip", "172.17.0.3");
    keepEqualLabels.Set("__meta_kubernetes_pod_label_app", "node-exporter");

    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configList = RelabelConfigList();
    APSARA_TEST_TRUE(configList.Init(configJson));
    auto result = keepEqualLabels;
    configList.Process(result);
    APSARA_TEST_EQUAL((size_t)3, result.Size());
    APSARA_TEST_EQUAL("172.17.0.3", result.Get("__meta_kubernetes_pod_ip"));
}

void RelabelConfigUnittest::TestLowerCase() {
    Json::Value configJson;
    string configStr;
    string errorMsg;
    RelabelConfigList configList;
    // relabel lowercase
    configStr = R"(
        [{
                "action": "lowercase",
                "regex": "172.*",
                "source_labels": [
                    "__meta_kubernetes_pod_label_app"
                ],
                "target_label": "__meta_kubernetes_pod_label_app"
        }]
    )";
    Labels lowercaseLabels;
    lowercaseLabels.Set("__meta_kubernetes_pod_ip", "172.17.0.3");
    lowercaseLabels.Set("pod_ip", "172.17.0.3");
    lowercaseLabels.Set("__meta_kubernetes_pod_label_app", "node-Exporter");

    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configList = RelabelConfigList();
    APSARA_TEST_TRUE(configList.Init(configJson));
    auto result = lowercaseLabels;
    configList.Process(result);
    APSARA_TEST_EQUAL((size_t)3, result.Size());
    APSARA_TEST_EQUAL("node-exporter", result.Get("__meta_kubernetes_pod_label_app"));
}

void RelabelConfigUnittest::TestUpperCase() {
    Json::Value configJson;
    string configStr;
    string errorMsg;
    RelabelConfigList configList;
    // relabel uppercase
    configStr = R"(
        [{
                "action": "uppercase",
                "regex": "172.*",
                "source_labels": [
                    "__meta_kubernetes_pod_label_app"
                ],
                "target_label": "__meta_kubernetes_pod_label_app"
        }]
    )";
    Labels uppercaseLabels;
    uppercaseLabels.Set("__meta_kubernetes_pod_ip", "172.17.0.3");
    uppercaseLabels.Set("pod_ip", "172.17.0.3");
    uppercaseLabels.Set("__meta_kubernetes_pod_label_app", "node-Exporter");

    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configList = RelabelConfigList();
    APSARA_TEST_TRUE(configList.Init(configJson));
    auto result = uppercaseLabels;
    configList.Process(result);
    APSARA_TEST_EQUAL((size_t)3, result.Size());
    APSARA_TEST_EQUAL("NODE-EXPORTER", result.Get("__meta_kubernetes_pod_label_app"));
}

void RelabelConfigUnittest::TestHashMod() {
    Json::Value configJson;
    string configStr;
    string errorMsg;
    RelabelConfigList configList;

    // relabel hashmod
    configStr = R"(
        [{
                "action": "hashmod",
                "regex": "172.*",
                "source_labels": [
                    "__meta_kubernetes_pod_label_app"
                ],
                "target_label": "hash_val",
                "modulus": 255
        }]
    )";
    Labels hashmodLabels;
    hashmodLabels.Set("__meta_kubernetes_pod_ip", "172.17.0.3");
    hashmodLabels.Set("pod_ip", "172.17.0.3");
    hashmodLabels.Set("__meta_kubernetes_pod_label_app", "node-Exporter");

    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configList = RelabelConfigList();
    APSARA_TEST_TRUE(configList.Init(configJson));
    auto result = hashmodLabels;
    configList.Process(result);
    APSARA_TEST_EQUAL((size_t)4, result.Size());
    APSARA_TEST_TRUE(!result.Get("hash_val").empty());
}

void RelabelConfigUnittest::TestLabelMap() {
    Json::Value configJson;
    string configStr;
    string errorMsg;
    RelabelConfigList configList;
    Labels labels;
    labels.Set("__meta_kubernetes_pod_ip", "172.17.0.3");
    labels.Set("__meta_kubernetes_pod_label_app", "node-exporter");
    // single relabel labelmap
    configStr = R"JSON(
        [{
                "action": "labelmap",
                "regex": "__meta_kubernetes_pod_label_(.+)",
                "replacement": "k8s_$1"
        }]
    )JSON";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configList = RelabelConfigList();
    APSARA_TEST_TRUE(configList.Init(configJson));
    auto result = labels;
    configList.Process(result);
    APSARA_TEST_EQUAL((size_t)3, result.Size());
    APSARA_TEST_EQUAL("node-exporter", result.Get("k8s_app"));
}

void RelabelConfigUnittest::TestLabelDrop() {
    Json::Value configJson;
    string configStr;
    string errorMsg;
    RelabelConfigList configList;

    // relabel labeldrop
    configStr = R"(
        [{
                "action": "labeldrop",
                "regex": "__meta.*"
        }]
    )";
    Labels labelDropLabels;
    labelDropLabels.Set("__meta_kubernetes_pod_ip", "172.17.0.3");
    labelDropLabels.Set("pod_ip", "172.17.0.3");
    labelDropLabels.Set("__meta_kubernetes_pod_label_app", "node-Exporter");

    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configList = RelabelConfigList();
    APSARA_TEST_TRUE(configList.Init(configJson));
    auto result = labelDropLabels;
    configList.Process(result);
    APSARA_TEST_EQUAL((size_t)1, result.Size());
    APSARA_TEST_EQUAL("172.17.0.3", result.Get("pod_ip"));
}

void RelabelConfigUnittest::TestLabelKeep() {
    Json::Value configJson;
    string configStr;
    string errorMsg;
    RelabelConfigList configList;

    // relabel labelkeep
    configStr = R"(
        [{
                "action": "labelkeep",
                "regex": "__meta.*"
        }]
    )";
    Labels labelKeepLabels;
    labelKeepLabels.Set("__meta_kubernetes_pod_ip", "172.17.0.3");
    labelKeepLabels.Set("pod_ip", "172.17.0.3");
    labelKeepLabels.Set("__meta_kubernetes_pod_label_app", "node-exporter");

    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configList = RelabelConfigList();
    APSARA_TEST_TRUE(configList.Init(configJson));
    auto result = labelKeepLabels;
    configList.Process(result);
    APSARA_TEST_EQUAL((size_t)2, result.Size());
    APSARA_TEST_EQUAL("172.17.0.3", result.Get("__meta_kubernetes_pod_ip"));
    APSARA_TEST_EQUAL("node-exporter", result.Get("__meta_kubernetes_pod_label_app"));
}

void RelabelConfigUnittest::TestMultiRelabel() {
    Json::Value configJson;
    string configStr;
    string errorMsg;
    RelabelConfigList configList;
    Labels labels;
    labels.Set("__meta_kubernetes_pod_ip", "172.17.0.3");
    labels.Set("__meta_kubernetes_pod_label_app", "node-exporter");

    // multi relabel
    string configStr1;
    string configStr2;
    configStr1 = configStr1 + R"JSON(
        [{
                "action": "replace",
                "regex": "(.*)",
                "replacement": "${1}:9100",
                "separator": ";",
                "source_labels": [
                    "__meta_kubernetes_pod_ip"
                ],
                "target_label": "__address__"
        }]
    )JSON";
    configStr2 = R"(
        [{
                "action": "drop",
                "regex": "172.*",
                "separator": ";",
                "source_labels": [
                    "__address__"
                ]
        }]
    )";

    APSARA_TEST_TRUE(ParseJsonTable(configStr1, configJson, errorMsg));
    configList = RelabelConfigList();
    APSARA_TEST_TRUE(configList.Init(configJson));
    auto result = labels;
    APSARA_TEST_TRUE(configList.Process(result));
    APSARA_TEST_EQUAL((size_t)3, result.Size());
    APSARA_TEST_EQUAL("172.17.0.3:9100", result.Get("__address__"));

    APSARA_TEST_TRUE(ParseJsonTable(configStr2, configJson, errorMsg));
    configList = RelabelConfigList();
    APSARA_TEST_TRUE(configList.Init(configJson));
    // result = labels;
    configList.Process(result);
}

UNIT_TEST_CASE(ActionConverterUnittest, TestStringToAction)
UNIT_TEST_CASE(ActionConverterUnittest, TestActionToString)

UNIT_TEST_CASE(RelabelConfigUnittest, TestRelabelConfig)
UNIT_TEST_CASE(RelabelConfigUnittest, TestReplace)
UNIT_TEST_CASE(RelabelConfigUnittest, TestDrop)
UNIT_TEST_CASE(RelabelConfigUnittest, TestKeep)
UNIT_TEST_CASE(RelabelConfigUnittest, TestHashMod)
UNIT_TEST_CASE(RelabelConfigUnittest, TestLabelMap)
UNIT_TEST_CASE(RelabelConfigUnittest, TestLabelDrop)
UNIT_TEST_CASE(RelabelConfigUnittest, TestLabelKeep)
UNIT_TEST_CASE(RelabelConfigUnittest, TestDropEqual)
UNIT_TEST_CASE(RelabelConfigUnittest, TestKeepEqual)
UNIT_TEST_CASE(RelabelConfigUnittest, TestLowerCase)
UNIT_TEST_CASE(RelabelConfigUnittest, TestUpperCase)
UNIT_TEST_CASE(RelabelConfigUnittest, TestMultiRelabel)

} // namespace logtail

UNIT_TEST_MAIN
