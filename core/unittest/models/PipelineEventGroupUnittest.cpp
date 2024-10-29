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
#include "models/EventPool.h"
#include "models/PipelineEventGroup.h"
#include "runner/ProcessorRunner.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class PipelineEventGroupUnittest : public ::testing::Test {
public:
    void TestCreateEvent();
    void TestAddEvent();
    void TestSwapEvents();
    void TestReserveEvents();
    void TestCopy();
    void TestDestructor();
    void TestSetMetadata();
    void TestDelMetadata();
    void TestFromJsonToJson();

protected:
    void SetUp() override {
        mSourceBuffer.reset(new SourceBuffer);
        mEventGroup.reset(new PipelineEventGroup(mSourceBuffer));
    }

    void TearDown() override {
        mEventGroup.reset();
        mPool.Clear();
        gThreadedEventPool.Clear();
    }

private:
    std::shared_ptr<SourceBuffer> mSourceBuffer;
    std::unique_ptr<PipelineEventGroup> mEventGroup;
    EventPool mPool;
};

void PipelineEventGroupUnittest::TestCreateEvent() {
    {
        auto logEvent = mEventGroup->CreateLogEvent();
        auto metricEvent = mEventGroup->CreateMetricEvent();
        auto spanEvent = mEventGroup->CreateSpanEvent();
        APSARA_TEST_EQUAL(mEventGroup.get(), logEvent->mPipelineEventGroupPtr);
        APSARA_TEST_EQUAL(mEventGroup.get(), metricEvent->mPipelineEventGroupPtr);
        APSARA_TEST_EQUAL(mEventGroup.get(), spanEvent->mPipelineEventGroupPtr);
    }
    {
        auto logEvent = mEventGroup->CreateLogEvent(true);
        auto metricEvent = mEventGroup->CreateMetricEvent(true);
        auto spanEvent = mEventGroup->CreateSpanEvent(true);
        APSARA_TEST_EQUAL(mEventGroup.get(), logEvent->mPipelineEventGroupPtr);
        APSARA_TEST_EQUAL(mEventGroup.get(), metricEvent->mPipelineEventGroupPtr);
        APSARA_TEST_EQUAL(mEventGroup.get(), spanEvent->mPipelineEventGroupPtr);
    }
    {
        auto logEvent = mEventGroup->CreateLogEvent(true, &mPool);
        auto metricEvent = mEventGroup->CreateMetricEvent(true, &mPool);
        auto spanEvent = mEventGroup->CreateSpanEvent(true, &mPool);
        APSARA_TEST_EQUAL(mEventGroup.get(), logEvent->mPipelineEventGroupPtr);
        APSARA_TEST_EQUAL(mEventGroup.get(), metricEvent->mPipelineEventGroupPtr);
        APSARA_TEST_EQUAL(mEventGroup.get(), spanEvent->mPipelineEventGroupPtr);
    }
}

void PipelineEventGroupUnittest::TestAddEvent() {
    {
        auto logEvent = mEventGroup->AddLogEvent();
        auto metricEvent = mEventGroup->AddMetricEvent();
        auto spanEvent = mEventGroup->AddSpanEvent();
        APSARA_TEST_EQUAL(mEventGroup.get(), logEvent->mPipelineEventGroupPtr);
        APSARA_TEST_EQUAL(mEventGroup.get(), metricEvent->mPipelineEventGroupPtr);
        APSARA_TEST_EQUAL(mEventGroup.get(), spanEvent->mPipelineEventGroupPtr);
    }
    {
        auto logEvent = mEventGroup->AddLogEvent(true);
        auto metricEvent = mEventGroup->AddMetricEvent(true);
        auto spanEvent = mEventGroup->AddSpanEvent(true);
        APSARA_TEST_EQUAL(mEventGroup.get(), logEvent->mPipelineEventGroupPtr);
        APSARA_TEST_EQUAL(mEventGroup.get(), metricEvent->mPipelineEventGroupPtr);
        APSARA_TEST_EQUAL(mEventGroup.get(), spanEvent->mPipelineEventGroupPtr);
    }
    {
        auto logEvent = mEventGroup->AddLogEvent(true, &mPool);
        auto metricEvent = mEventGroup->AddMetricEvent(true, &mPool);
        auto spanEvent = mEventGroup->AddSpanEvent(true, &mPool);
        APSARA_TEST_EQUAL(mEventGroup.get(), logEvent->mPipelineEventGroupPtr);
        APSARA_TEST_EQUAL(mEventGroup.get(), metricEvent->mPipelineEventGroupPtr);
        APSARA_TEST_EQUAL(mEventGroup.get(), spanEvent->mPipelineEventGroupPtr);
    }
}

void PipelineEventGroupUnittest::TestSwapEvents() {
    mEventGroup->AddLogEvent();
    mEventGroup->AddMetricEvent();
    mEventGroup->AddSpanEvent();
    EventsContainer eventContainer;
    mEventGroup->SwapEvents(eventContainer);
    APSARA_TEST_EQUAL_FATAL(3U, eventContainer.size());
    APSARA_TEST_EQUAL_FATAL(0U, mEventGroup->GetEvents().size());
}

void PipelineEventGroupUnittest::TestDestructor() {
    LogEvent* log = nullptr;
    MetricEvent* metric = nullptr;
    SpanEvent* span = nullptr;
    {
        PipelineEventGroup g(make_shared<SourceBuffer>());
        log = g.AddLogEvent(true);
        log->SetTimestamp(1234567890);
    }
    APSARA_TEST_EQUAL(1U, gThreadedEventPool.mLogEventPool.size());
    APSARA_TEST_EQUAL(log, gThreadedEventPool.mLogEventPool.back());
    APSARA_TEST_EQUAL(0, log->GetTimestamp());
    {
        PipelineEventGroup g(make_shared<SourceBuffer>());
        metric = g.AddMetricEvent(true);
        metric->SetTimestamp(1234567890);
    }
    APSARA_TEST_EQUAL(1U, gThreadedEventPool.mMetricEventPool.size());
    APSARA_TEST_EQUAL(metric, gThreadedEventPool.mMetricEventPool.back());
    APSARA_TEST_EQUAL(0, metric->GetTimestamp());
    {
        PipelineEventGroup g(make_shared<SourceBuffer>());
        span = g.AddSpanEvent(true);
        span->SetTimestamp(1234567890);
    }
    APSARA_TEST_EQUAL(1U, gThreadedEventPool.mSpanEventPool.size());
    APSARA_TEST_EQUAL(span, gThreadedEventPool.mSpanEventPool.back());
    APSARA_TEST_EQUAL(0, span->GetTimestamp());
    {
        PipelineEventGroup g(make_shared<SourceBuffer>());
        log = g.AddLogEvent(true, &mPool);
        log->SetTimestamp(1234567890);
    }
    APSARA_TEST_EQUAL(1U, mPool.mLogEventPoolBak.size());
    APSARA_TEST_EQUAL(log, mPool.mLogEventPoolBak.back());
    APSARA_TEST_EQUAL(0, log->GetTimestamp());
    {
        PipelineEventGroup g(make_shared<SourceBuffer>());
        metric = g.AddMetricEvent(true, &mPool);
        metric->SetTimestamp(1234567890);
    }
    APSARA_TEST_EQUAL(1U, mPool.mMetricEventPoolBak.size());
    APSARA_TEST_EQUAL(metric, mPool.mMetricEventPoolBak.back());
    APSARA_TEST_EQUAL(0, metric->GetTimestamp());
    {
        PipelineEventGroup g(make_shared<SourceBuffer>());
        span = g.AddSpanEvent(true, &mPool);
        span->SetTimestamp(1234567890);
    }
    APSARA_TEST_EQUAL(1U, mPool.mSpanEventPoolBak.size());
    APSARA_TEST_EQUAL(span, mPool.mSpanEventPoolBak.back());
    APSARA_TEST_EQUAL(0, span->GetTimestamp());
}

void PipelineEventGroupUnittest::TestReserveEvents() {
    mEventGroup->ReserveEvents(10);
    APSARA_TEST_EQUAL(10U, mEventGroup->GetEvents().capacity());
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

UNIT_TEST_CASE(PipelineEventGroupUnittest, TestCreateEvent)
UNIT_TEST_CASE(PipelineEventGroupUnittest, TestAddEvent)
UNIT_TEST_CASE(PipelineEventGroupUnittest, TestSwapEvents)
UNIT_TEST_CASE(PipelineEventGroupUnittest, TestReserveEvents)
UNIT_TEST_CASE(PipelineEventGroupUnittest, TestCopy)
UNIT_TEST_CASE(PipelineEventGroupUnittest, TestDestructor)
UNIT_TEST_CASE(PipelineEventGroupUnittest, TestSetMetadata)
UNIT_TEST_CASE(PipelineEventGroupUnittest, TestDelMetadata)
UNIT_TEST_CASE(PipelineEventGroupUnittest, TestFromJsonToJson)

} // namespace logtail

UNIT_TEST_MAIN
