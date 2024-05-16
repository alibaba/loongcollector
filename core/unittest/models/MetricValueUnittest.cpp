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
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class MetricValueUnittest : public ::testing::Test {
public:
    void TestToJson();
    void TestFromJson();
};

void MetricValueUnittest::TestToJson() {
    MetricValue value = UntypedSingleValue{10.0};
    Json::Value res = MetricValueToJson(value);

    Json::Value valueJson;
    string valueStr = R"({
        "type": "untyped_single_value",
        "detail": 10.0
    })";
    string errorMsg;
    ParseJsonTable(valueStr, valueJson, errorMsg);

    APSARA_TEST_TRUE(valueJson == res);
}

void MetricValueUnittest::TestFromJson() {
    Json::Value detail(10.0);
    MetricValue value = JsonToMetricValue("untyped_single_value", detail);

    APSARA_TEST_TRUE(std::holds_alternative<UntypedSingleValue>(value));
    APSARA_TEST_EQUAL(10.0, std::get<UntypedSingleValue>(value).mValue);
}

UNIT_TEST_CASE(MetricValueUnittest, TestToJson)
UNIT_TEST_CASE(MetricValueUnittest, TestFromJson)

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

} // namespace logtail

UNIT_TEST_MAIN
