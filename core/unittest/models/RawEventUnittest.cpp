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

#include "common/JsonUtil.h"
#include "models/RawEvent.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class RawEventUnittest : public ::testing::Test {
public:
    void TestTimestampOp();
    void TestSetContent();
    void TestSize();
    void TestReset();
    void TestFromJson();
    void TestToJson();

protected:
    void SetUp() override {
        mSourceBuffer.reset(new SourceBuffer);
        mEventGroup.reset(new PipelineEventGroup(mSourceBuffer));
        mRawEvent = mEventGroup->CreateRawEvent();
    }

private:
    shared_ptr<SourceBuffer> mSourceBuffer;
    unique_ptr<PipelineEventGroup> mEventGroup;
    unique_ptr<RawEvent> mRawEvent;
};

void RawEventUnittest::TestTimestampOp() {
    mRawEvent->SetTimestamp(13800000000);
    APSARA_TEST_EQUAL(13800000000, mRawEvent->GetTimestamp());
}

void RawEventUnittest::TestSetContent() {
    mRawEvent->SetContent(string("content"));
    APSARA_TEST_EQUAL("content", mRawEvent->GetContent().to_string());
}

void RawEventUnittest::TestSize() {
    size_t basicSize = sizeof(time_t) + sizeof(long);
    mRawEvent->SetContent(string("content"));
    APSARA_TEST_EQUAL(basicSize + 7U, mRawEvent->DataSize());
}

void RawEventUnittest::TestReset() {
    mRawEvent->SetTimestamp(12345678901);
    mRawEvent->SetContent(string("content"));
    mRawEvent->Reset();
    APSARA_TEST_EQUAL(0, mRawEvent->GetTimestamp());
    APSARA_TEST_FALSE(mRawEvent->GetTimestampNanosecond().has_value());
    APSARA_TEST_TRUE(mRawEvent->GetContent().empty());
}

void RawEventUnittest::TestFromJson() {
    Json::Value eventJson;
    string eventStr = R"({
        "timestamp" : 12345678901,
        "timestampNanosecond" : 0,
        "content": "hello, world!"
    })";
    string errorMsg;
    ParseJsonTable(eventStr, eventJson, errorMsg);
    mRawEvent->FromJson(eventJson);

    APSARA_TEST_EQUAL(12345678901, mRawEvent->GetTimestamp());
    APSARA_TEST_EQUAL(0L, mRawEvent->GetTimestampNanosecond().value());
    APSARA_TEST_EQUAL("hello, world!", mRawEvent->GetContent().to_string());
}

void RawEventUnittest::TestToJson() {
    mRawEvent->SetTimestamp(12345678901, 0);
    mRawEvent->SetContent("hello, world!");
    Json::Value res = mRawEvent->ToJson();

    Json::Value eventJson;
    string eventStr = R"({
        "content": "hello, world!",
        "timestamp" : 12345678901,
        "timestampNanosecond" : 0,
        "type": 4
    })";
    string errorMsg;
    ParseJsonTable(eventStr, eventJson, errorMsg);

    APSARA_TEST_TRUE(eventJson == res);
}

UNIT_TEST_CASE(RawEventUnittest, TestTimestampOp)
UNIT_TEST_CASE(RawEventUnittest, TestSetContent)
UNIT_TEST_CASE(RawEventUnittest, TestSize)
UNIT_TEST_CASE(RawEventUnittest, TestReset)
UNIT_TEST_CASE(RawEventUnittest, TestFromJson)
UNIT_TEST_CASE(RawEventUnittest, TestToJson)

} // namespace logtail

UNIT_TEST_MAIN
