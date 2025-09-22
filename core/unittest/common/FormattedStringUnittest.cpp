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
#include "common/memory/SourceBuffer.h"
#include "models/LogEvent.h"
#include "models/PipelineEventGroup.h"
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

    std::string result;
    PipelineEventGroup group(std::make_shared<SourceBuffer>());
    auto* event = group.AddLogEvent();
    (void)event;
    APSARA_TEST_TRUE(formatter.Format(group.GetEvents().back(), group.GetTags(), result));
    APSARA_TEST_EQUAL(std::string("plain_topic"), result);
}

void FormattedStringUnittest::TestDynamicTemplate() {
    FormattedString formatter;
    APSARA_TEST_TRUE(formatter.Init("logs_%{tag.namespace}_%{content.app}"));
    APSARA_TEST_TRUE(formatter.IsDynamic());

    std::string result;
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup group(sourceBuffer);
    auto* event = group.AddLogEvent();
    event->SetContent(StringView("app"), StringView("frontend"));
    group.SetTag(StringView("namespace"), StringView("access"));
    APSARA_TEST_TRUE(formatter.Format(group.GetEvents().back(), group.GetTags(), result));
    APSARA_TEST_EQUAL(std::string("logs_access_frontend"), result);
}

void FormattedStringUnittest::TestMissingKey() {
    FormattedString formatter;
    APSARA_TEST_TRUE(formatter.Init("logs_%{tag.namespace}"));

    std::string result;
    PipelineEventGroup group(std::make_shared<SourceBuffer>());
    auto* event = group.AddLogEvent();
    (void)event;
    APSARA_TEST_FALSE(formatter.Format(group.GetEvents().back(), group.GetTags(), result));
}

void FormattedStringUnittest::TestEnvironmentPlaceholder() {
    const char* envKey = "FORMATTED_STRING_ENV";

#if defined(_WIN32) || defined(_WIN64)
    _putenv_s(envKey, "prod");
#else
    setenv(envKey, "prod", 1);
#endif

    FormattedString formatter;
    APSARA_TEST_TRUE(formatter.Init("service_${FORMATTED_STRING_ENV}_%{tag.env}"));

    std::string result;
    PipelineEventGroup group(std::make_shared<SourceBuffer>());
    auto* event = group.AddLogEvent();
    (void)event;
    group.SetTag(StringView("env"), StringView("blue"));
    APSARA_TEST_TRUE(formatter.Format(group.GetEvents().back(), group.GetTags(), result));
    APSARA_TEST_EQUAL(std::string("service_prod_blue"), result);
}

UNIT_TEST_CASE(FormattedStringUnittest, TestStaticTemplate)
UNIT_TEST_CASE(FormattedStringUnittest, TestDynamicTemplate)
UNIT_TEST_CASE(FormattedStringUnittest, TestMissingKey)
UNIT_TEST_CASE(FormattedStringUnittest, TestEnvironmentPlaceholder)

} // namespace logtail

UNIT_TEST_MAIN
