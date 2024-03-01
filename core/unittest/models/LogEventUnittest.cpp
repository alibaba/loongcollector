// Copyright 2023 iLogtail Authors
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

#include <cstdlib>

#include "common/JsonUtil.h"
#include "models/LogEvent.h"
#include "unittest/Unittest.h"

namespace logtail {

class LogEventUnittest : public ::testing::Test {
public:
    void TestSetTimestamp();
    void TestSetContent();
    void TestDelContent();

    void TestFromJsonToJson();

protected:
    void SetUp() override {
        mSourceBuffer.reset(new SourceBuffer);
        mEventGroup.reset(new PipelineEventGroup(mSourceBuffer));
        mLogEvent = mEventGroup->CreateLogEvent();
    }

private:
    std::shared_ptr<SourceBuffer> mSourceBuffer;
    std::unique_ptr<PipelineEventGroup> mEventGroup;
    std::unique_ptr<LogEvent> mLogEvent;
};

UNIT_TEST_CASE(LogEventUnittest, TestSetTimestamp)
UNIT_TEST_CASE(LogEventUnittest, TestSetContent)
UNIT_TEST_CASE(LogEventUnittest, TestDelContent)
UNIT_TEST_CASE(LogEventUnittest, TestFromJsonToJson)

void LogEventUnittest::TestSetTimestamp() {
    mLogEvent->SetTimestamp(13800000000);
    APSARA_TEST_EQUAL_FATAL(13800000000, mLogEvent->GetTimestamp());
}

void LogEventUnittest::TestSetContent() {
    { // string copy, let kv out of scope
        mLogEvent->SetContent(std::string("key1"), std::string("value1"));
    }
    { // stringview copy, let kv out of scope
        std::string key("key2");
        std::string value("value2");
        mLogEvent->SetContent(StringView(key), StringView(value));
    }
    size_t beforeAlloc;
    { // StringBuffer nocopy
        StringBuffer key = mLogEvent->GetSourceBuffer()->CopyString(std::string("key3"));
        StringBuffer value = mLogEvent->GetSourceBuffer()->CopyString(std::string("value3"));
        beforeAlloc = mSourceBuffer->mAllocator.TotalAllocated();
        mLogEvent->SetContentNoCopy(key, value);
    }
    std::string key("key4");
    std::string value("value4");
    { // StringView nocopy
        mLogEvent->SetContentNoCopy(StringView(key), StringView(value));
    }
    size_t afterAlloc = mSourceBuffer->mAllocator.TotalAllocated();
    APSARA_TEST_EQUAL_FATAL(beforeAlloc, afterAlloc);
    std::vector<std::pair<std::string, std::string>> answers = {
        {"key1", "value1"},
        {"key2", "value2"},
        {"key3", "value3"},
        {"key4", "value4"},
    };
    for (const auto kv : answers) {
        APSARA_TEST_TRUE_FATAL(mLogEvent->HasContent(kv.first));
        APSARA_TEST_STREQ_FATAL(kv.second.c_str(), mLogEvent->GetContent(kv.first).data());
    }
}

void LogEventUnittest::TestDelContent() {
    mLogEvent->SetContent(std::string("key1"), std::string("value1"));
    APSARA_TEST_TRUE_FATAL(mLogEvent->HasContent("key1"));
    mLogEvent->DelContent(std::string("key1"));
    APSARA_TEST_FALSE_FATAL(mLogEvent->HasContent("key1"));
}

void LogEventUnittest::TestFromJsonToJson() {
    std::string inJson = R"({
        "contents" :
        {
            "key1" : "value1",
            "key2" : "value2"
        },
        "timestamp" : 12345678901,
        "timestampNanosecond" : 0,
        "type" : 1
    })";
    APSARA_TEST_TRUE_FATAL(mLogEvent->FromJsonString(inJson));
    APSARA_TEST_EQUAL_FATAL(12345678901L, mLogEvent->GetTimestamp());
    std::vector<std::pair<std::string, std::string>> answers = {{"key1", "value1"}, {"key2", "value2"}};
    for (const auto kv : answers) {
        APSARA_TEST_TRUE_FATAL(mLogEvent->HasContent(kv.first));
        APSARA_TEST_STREQ_FATAL(kv.second.c_str(), mLogEvent->GetContent(kv.first).data());
    }
    std::string outJson = mLogEvent->ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(inJson).c_str(), CompactJson(outJson).c_str());
}

} // namespace logtail

UNIT_TEST_MAIN