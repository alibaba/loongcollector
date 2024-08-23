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
#include "models/PipelineEventGroup.h"
#include "unittest/Unittest.h"

namespace logtail {

class PipelineEventGroupUnittest : public ::testing::Test {
public:
    void TestSwapEvents();
    void TestCopy();
    void TestSetMetadata();
    void TestDelMetadata();
    void TestFromJsonToJson();

protected:
    void SetUp() override {
        mSourceBuffer.reset(new SourceBuffer);
        mEventGroup.reset(new PipelineEventGroup(mSourceBuffer));
    }

private:
    std::shared_ptr<SourceBuffer> mSourceBuffer;
    std::unique_ptr<PipelineEventGroup> mEventGroup;
};

void PipelineEventGroupUnittest::TestSwapEvents() {
    mEventGroup->AddLogEvent();
    mEventGroup->AddMetricEvent();
    mEventGroup->AddSpanEvent();
    EventsContainer eventContainer;
    mEventGroup->SwapEvents(eventContainer);
    APSARA_TEST_EQUAL_FATAL(3U, eventContainer.size());
    APSARA_TEST_EQUAL_FATAL(0U, mEventGroup->GetEvents().size());
}

void PipelineEventGroupUnittest::TestCopy() {
    mEventGroup->AddLogEvent();
    auto res = mEventGroup->Copy();
    APSARA_TEST_EQUAL(1U, res.GetEvents().size());
    APSARA_TEST_EQUAL(&res, res.GetEvents()[0]->mPipelineEventGroupPtr);
    APSARA_TEST_EQUAL(3U, res.GetSourceBuffer().use_count());
}

void PipelineEventGroupUnittest::TestSetMetadata() {
    { // string copy, let kv out of scope
        mEventGroup->SetMetadata(EventGroupMetaKey::LOG_FILE_PATH, std::string("value1"));
    }
    { // stringview copy, let kv out of scope
        std::string value("value2");
        mEventGroup->SetMetadata(EventGroupMetaKey::LOG_FILE_PATH_RESOLVED, StringView(value));
    }
    size_t beforeAlloc;
    { // StringBuffer nocopy
        StringBuffer value = mEventGroup->GetSourceBuffer()->CopyString(std::string("value3"));
        beforeAlloc = mSourceBuffer->mAllocator.TotalAllocated();
        mEventGroup->SetMetadataNoCopy(EventGroupMetaKey::LOG_FILE_INODE, value);
    }
    std::string value("value4");
    { // StringView nocopy
        mEventGroup->SetMetadataNoCopy(EventGroupMetaKey::SOURCE_ID, StringView(value));
    }
    size_t afterAlloc = mSourceBuffer->mAllocator.TotalAllocated();
    APSARA_TEST_EQUAL_FATAL(beforeAlloc, afterAlloc);
    std::vector<std::pair<EventGroupMetaKey, std::string>> answers
        = {{EventGroupMetaKey::LOG_FILE_PATH, "value1"},
           {EventGroupMetaKey::LOG_FILE_PATH_RESOLVED, "value2"},
           {EventGroupMetaKey::LOG_FILE_INODE, "value3"},
           {EventGroupMetaKey::SOURCE_ID, "value4"}};
    for (const auto kv : answers) {
        APSARA_TEST_TRUE_FATAL(mEventGroup->HasMetadata(kv.first));
        APSARA_TEST_STREQ_FATAL(kv.second.c_str(), mEventGroup->GetMetadata(kv.first).data());
    }
}

void PipelineEventGroupUnittest::TestDelMetadata() {
    mEventGroup->SetMetadata(EventGroupMetaKey::LOG_FILE_INODE, std::string("value1"));
    APSARA_TEST_TRUE_FATAL(mEventGroup->HasMetadata(EventGroupMetaKey::LOG_FILE_INODE));
    mEventGroup->DelMetadata(EventGroupMetaKey::LOG_FILE_INODE);
    APSARA_TEST_FALSE_FATAL(mEventGroup->HasMetadata(EventGroupMetaKey::LOG_FILE_INODE));
}

void PipelineEventGroupUnittest::TestFromJsonToJson() {
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "key1" : "value1",
                    "key2" : "value2"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ],
        "metadata" :
        {
            "log.file.path" : "/var/log/message"
        },
        "tags" :
        {
            "app_name" : "xxx"
        }
    })";
    APSARA_TEST_TRUE_FATAL(mEventGroup->FromJsonString(inJson));
    auto& events = mEventGroup->GetEvents();
    APSARA_TEST_EQUAL_FATAL(1U, events.size());
    auto& logEvent = events[0];
    APSARA_TEST_TRUE_FATAL(logEvent.Is<LogEvent>());

    APSARA_TEST_TRUE_FATAL(mEventGroup->HasMetadata(EventGroupMetaKey::LOG_FILE_PATH));
    APSARA_TEST_STREQ_FATAL("/var/log/message", mEventGroup->GetMetadata(EventGroupMetaKey::LOG_FILE_PATH).data());

    APSARA_TEST_TRUE_FATAL(mEventGroup->HasTag("app_name"));
    APSARA_TEST_STREQ_FATAL("xxx", mEventGroup->GetTag("app_name").data());

    std::string outJson = mEventGroup->ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(inJson).c_str(), CompactJson(outJson).c_str());
}

UNIT_TEST_CASE(PipelineEventGroupUnittest, TestSwapEvents)
UNIT_TEST_CASE(PipelineEventGroupUnittest, TestCopy)
UNIT_TEST_CASE(PipelineEventGroupUnittest, TestSetMetadata)
UNIT_TEST_CASE(PipelineEventGroupUnittest, TestDelMetadata)
UNIT_TEST_CASE(PipelineEventGroupUnittest, TestFromJsonToJson)

} // namespace logtail

UNIT_TEST_MAIN
