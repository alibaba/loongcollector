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

#include <cstdint>
#include <string>

#include "prometheus/Constants.h"
#include "prometheus/labels/Labels.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
class LabelsUnittest : public testing::Test {
public:
    void TestGet();
    void TestSet();
    void TestRange();
    void TestHash();
    void TestRemoveMetaLabels();

private:
};

class LabelsBuilderUnittest : public testing::Test {
public:
    void TestReset();
    void TestDeleteLabel();
    void TestSet();
    void TestGet();
    void TestLabels();
    void TestRange();
};

void LabelsUnittest::TestRemoveMetaLabels() {
    Labels labels;
    labels.Set("host", "172.17.0.3:9100");
    labels.Set("__meta_port", "172.17.0.3");
    labels.Set("port", "9100");
    APSARA_TEST_EQUAL(3UL, labels.Size());

    labels.RemoveMetaLabels();
    APSARA_TEST_EQUAL(2UL, labels.Size());
    APSARA_TEST_EQUAL("", labels.Get("__meta_port"));
}

void LabelsUnittest::TestHash() {
    Labels labels;

    labels.Set("host", "172.17.0.3:9100");
    labels.Set("ip", "172.17.0.3");
    labels.Set("port", "9100");
    uint64_t hash = labels.Hash();

    uint64_t expect = prometheus::OFFSET64;
    string raw;
    raw = raw + "host" + "\xff" + "172.17.0.3:9100" + "\xff" + "ip" + "\xff" + "172.17.0.3" + "\xff" + "port" + "\xff"
        + "9100" + "\xff";
    for (auto i : raw) {
        expect ^= (uint64_t)i;
        expect *= prometheus::PRIME64;
    }

    APSARA_TEST_EQUAL(expect, hash);
}

void LabelsUnittest::TestGet() {
    Labels labels;
    labels.Set("host", "172.17.0.3:9100");
    APSARA_TEST_EQUAL(1UL, labels.Size());

    APSARA_TEST_EQUAL("", labels.Get("hosts"));

    APSARA_TEST_EQUAL("172.17.0.3:9100", labels.Get("host"));
}

void LabelsUnittest::TestSet() {
    Labels labels;

    labels.Set("host", "172.17.0.3:9100");

    APSARA_TEST_EQUAL("172.17.0.3:9100", labels.Get("host"));
}

void LabelsUnittest::TestRange() {
    Labels labels;
    map<string, string> testMap;
    testMap["host"] = "172.17.0.3:9100";
    testMap["ip"] = "172.17.0.3";
    testMap["port"] = "9100";

    labels.Set("host", "172.17.0.3:9100");
    labels.Set("ip", "172.17.0.3");
    labels.Set("port", "9100");

    map<string, string> resMap;
    labels.Range([&resMap](const string& k, const string& v) { resMap[k] = v; });

    APSARA_TEST_EQUAL(testMap, resMap);
}




UNIT_TEST_CASE(LabelsUnittest, TestGet)
UNIT_TEST_CASE(LabelsUnittest, TestSet)
UNIT_TEST_CASE(LabelsUnittest, TestRange)
UNIT_TEST_CASE(LabelsUnittest, TestHash)
UNIT_TEST_CASE(LabelsUnittest, TestRemoveMetaLabels)


} // namespace logtail

UNIT_TEST_MAIN