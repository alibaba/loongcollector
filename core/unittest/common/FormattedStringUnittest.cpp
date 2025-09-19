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

#include <cstdlib>

#include <string>
#include <unordered_map>

#include "common/FormattedString.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class FormattedStringUnittest : public testing::Test {
public:
    void TestStaticTemplate();
    void TestDynamicTemplate();
    void TestMissingKey();
    void TestEnvironmentPlaceholder();
};

void FormattedStringUnittest::TestStaticTemplate() {
    FormattedString formatter;
    APSARA_TEST_TRUE(formatter.Init("plain_topic"));
    APSARA_TEST_FALSE(formatter.IsDynamic());

    std::unordered_map<std::string, std::string> values;
    std::string result;
    APSARA_TEST_TRUE(formatter.Format(values, result));
    APSARA_TEST_EQUAL(std::string("plain_topic"), result);
}

void FormattedStringUnittest::TestDynamicTemplate() {
    FormattedString formatter;
    APSARA_TEST_TRUE(formatter.Init("logs_%{tag.namespace}_%{content.app}"));
    APSARA_TEST_TRUE(formatter.IsDynamic());

    std::unordered_map<std::string, std::string> values;
    values["tag.namespace"] = "access";
    values["content.app"] = "frontend";

    std::string result;
    APSARA_TEST_TRUE(formatter.Format(values, result));
    APSARA_TEST_EQUAL(std::string("logs_access_frontend"), result);
}

void FormattedStringUnittest::TestMissingKey() {
    FormattedString formatter;
    APSARA_TEST_TRUE(formatter.Init("logs_%{tag.namespace}"));

    std::unordered_map<std::string, std::string> values;
    std::string result;
    APSARA_TEST_FALSE(formatter.Format(values, result));
}

void FormattedStringUnittest::TestEnvironmentPlaceholder() {
    const char* envKey = "FORMATTED_STRING_ENV";
    setenv(envKey, "prod", 1);

    FormattedString formatter;
    APSARA_TEST_TRUE(formatter.Init("service_${FORMATTED_STRING_ENV}_%{tag.env}"));

    std::unordered_map<std::string, std::string> values;
    values["tag.env"] = "blue";

    std::string result;
    APSARA_TEST_TRUE(formatter.Format(values, result));
    APSARA_TEST_EQUAL(std::string("service_prod_blue"), result);
}

UNIT_TEST_CASE(FormattedStringUnittest, TestStaticTemplate)
UNIT_TEST_CASE(FormattedStringUnittest, TestDynamicTemplate)
UNIT_TEST_CASE(FormattedStringUnittest, TestMissingKey)
UNIT_TEST_CASE(FormattedStringUnittest, TestEnvironmentPlaceholder)

} // namespace logtail

UNIT_TEST_MAIN
