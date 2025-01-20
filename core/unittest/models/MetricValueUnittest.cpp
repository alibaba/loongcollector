// Copyright 2024 iLogtail Authors
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

#include "common/JsonUtil.h"
#include "models/MetricEvent.h"
#include "models/MetricValue.h"
#include "models/PipelineEventGroup.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class UntypedSingleValueUnittest : public ::testing::Test {
public:
    void TestToJson();
    void TestFromJson();
};

void UntypedSingleValueUnittest::TestToJson() {
    UntypedSingleValue value{10.0};
    Json::Value res = value.ToJson();

    Json::Value valueJson = Json::Value(10.0);

    APSARA_TEST_TRUE(valueJson == res);
}

void UntypedSingleValueUnittest::TestFromJson() {
    UntypedSingleValue value;
    value.FromJson(Json::Value(10.0));

    APSARA_TEST_EQUAL(10.0, value.mValue);
}

UNIT_TEST_CASE(UntypedSingleValueUnittest, TestToJson)
UNIT_TEST_CASE(UntypedSingleValueUnittest, TestFromJson)

class UntypedMultiDoubleValuesUnittest : public ::testing::Test {
public:
    void TestToJson();
    void TestFromJson();

protected:
    void SetUp() override {
        mSourceBuffer.reset(new SourceBuffer);
        mEventGroup.reset(new PipelineEventGroup(mSourceBuffer));
        mMetricEvent = mEventGroup->CreateMetricEvent();
    }

private:
    shared_ptr<SourceBuffer> mSourceBuffer;
    unique_ptr<PipelineEventGroup> mEventGroup;
    unique_ptr<MetricEvent> mMetricEvent;
};

void UntypedMultiDoubleValuesUnittest::TestToJson() {
    UntypedMultiDoubleValues value(mMetricEvent.get());
    value.SetValue(string("test-1"), {UntypedValueMetricType::MetricTypeCounter, 10.0});
    value.SetValue(string("test-2"), {UntypedValueMetricType::MetricTypeGauge, 2.0});
    Json::Value res = value.ToJson();

    Json::Value valueJson;
    valueJson["test-1"] = Json::Value();
    valueJson["test-1"]["type"] = "counter";
    valueJson["test-1"]["value"] = 10.0;
    valueJson["test-2"] = Json::Value();
    valueJson["test-2"]["type"] = "gauge";
    valueJson["test-2"]["value"] = 2.0;

    APSARA_TEST_TRUE(valueJson == res);
}

void UntypedMultiDoubleValuesUnittest::TestFromJson() {
    UntypedMultiDoubleValues value(mMetricEvent.get());
    Json::Value valueJson;
    valueJson["test-1"] = Json::Value();
    valueJson["test-1"]["type"] = "counter";
    valueJson["test-1"]["value"] = 10.0;
    valueJson["test-2"] = Json::Value();
    valueJson["test-2"]["type"] = "gauge";
    valueJson["test-2"]["value"] = 2.0;
    value.FromJson(valueJson);
    UntypedMultiDoubleValue val;

    APSARA_TEST_EQUAL(true, value.GetValue("test-1", val));
    APSARA_TEST_EQUAL(UntypedValueMetricType::MetricTypeCounter, val.MetricType);
    APSARA_TEST_EQUAL(10.0, val.Value);
    APSARA_TEST_EQUAL(true, value.GetValue("test-2", val));
    APSARA_TEST_EQUAL(UntypedValueMetricType::MetricTypeGauge, val.MetricType);
    APSARA_TEST_EQUAL(2.0, val.Value);
}

UNIT_TEST_CASE(UntypedMultiDoubleValuesUnittest, TestToJson)
UNIT_TEST_CASE(UntypedMultiDoubleValuesUnittest, TestFromJson)

} // namespace logtail

UNIT_TEST_MAIN
