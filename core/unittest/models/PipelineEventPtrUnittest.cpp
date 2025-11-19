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

#include "models/EventPool.h"
#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"
#include "unittest/Unittest.h"

namespace logtail {

class PipelineEventPtrUnittest : public ::testing::Test {
public:
    void TestIs();
    void TestGet();
    void TestCast();
    void TestRelease();
    void TestCopy();
    void TestDestruction();
    void TestAssignment();

protected:
    void SetUp() override {
        mSourceBuffer.reset(new SourceBuffer);
        mEventGroup.reset(new PipelineEventGroup(mSourceBuffer));
    }

private:
    std::shared_ptr<SourceBuffer> mSourceBuffer;
    std::unique_ptr<PipelineEventGroup> mEventGroup;
};

void PipelineEventPtrUnittest::TestIs() {
    PipelineEventPtr logEventPtr(mEventGroup->CreateLogEvent(), false, nullptr);
    PipelineEventPtr metricEventPtr(mEventGroup->CreateMetricEvent(), false, nullptr);
    PipelineEventPtr spanEventPtr(mEventGroup->CreateSpanEvent(), false, nullptr);
    PipelineEventPtr rawEventPtr(mEventGroup->CreateRawEvent(), false, nullptr);
    APSARA_TEST_TRUE_FATAL(logEventPtr.Is<LogEvent>());
    APSARA_TEST_FALSE_FATAL(logEventPtr.Is<MetricEvent>());
    APSARA_TEST_FALSE_FATAL(logEventPtr.Is<SpanEvent>());
    APSARA_TEST_FALSE_FATAL(logEventPtr.Is<RawEvent>());

    APSARA_TEST_FALSE_FATAL(metricEventPtr.Is<LogEvent>());
    APSARA_TEST_TRUE_FATAL(metricEventPtr.Is<MetricEvent>());
    APSARA_TEST_FALSE_FATAL(metricEventPtr.Is<SpanEvent>());
    APSARA_TEST_FALSE_FATAL(metricEventPtr.Is<RawEvent>());

    APSARA_TEST_FALSE_FATAL(spanEventPtr.Is<LogEvent>());
    APSARA_TEST_FALSE_FATAL(spanEventPtr.Is<MetricEvent>());
    APSARA_TEST_TRUE_FATAL(spanEventPtr.Is<SpanEvent>());
    APSARA_TEST_FALSE_FATAL(spanEventPtr.Is<RawEvent>());

    APSARA_TEST_FALSE_FATAL(rawEventPtr.Is<LogEvent>());
    APSARA_TEST_FALSE_FATAL(rawEventPtr.Is<MetricEvent>());
    APSARA_TEST_FALSE_FATAL(rawEventPtr.Is<SpanEvent>());
    APSARA_TEST_TRUE_FATAL(rawEventPtr.Is<RawEvent>());
}

void PipelineEventPtrUnittest::TestGet() {
    auto logUPtr = mEventGroup->CreateLogEvent();
    auto addr = logUPtr.get();
    PipelineEventPtr logEventPtr(std::move(logUPtr), false, nullptr);
    APSARA_TEST_EQUAL_FATAL(addr, logEventPtr.Get<LogEvent>());
}

void PipelineEventPtrUnittest::TestCast() {
    auto logUPtr = mEventGroup->CreateLogEvent();
    auto addr = logUPtr.get();
    PipelineEventPtr logEventPtr(std::move(logUPtr), false, nullptr);
    APSARA_TEST_EQUAL_FATAL(addr, &logEventPtr.Cast<LogEvent>());
}

void PipelineEventPtrUnittest::TestRelease() {
    auto logUPtr = mEventGroup->CreateLogEvent();
    auto* addr = logUPtr.get();
    PipelineEventPtr logEventPtr(std::move(logUPtr), false, nullptr);
    APSARA_TEST_EQUAL_FATAL(addr, logEventPtr.Release());
    delete addr;
}

void PipelineEventPtrUnittest::TestCopy() {
    mEventGroup->AddLogEvent();
    mEventGroup->AddMetricEvent();
    mEventGroup->AddSpanEvent();
    mEventGroup->AddRawEvent();
    {
        auto& event = mEventGroup->MutableEvents()[0];
        event->SetTimestamp(12345678901);
        auto res = event.Copy();
        APSARA_TEST_NOT_EQUAL(event.Get<LogEvent>(), res.Get<LogEvent>());
        APSARA_TEST_FALSE(res.IsFromEventPool());
        APSARA_TEST_EQUAL(nullptr, res.GetEventPool());
    }
    {
        auto& event = mEventGroup->MutableEvents()[1];
        event->SetTimestamp(12345678901);
        auto res = event.Copy();
        APSARA_TEST_NOT_EQUAL(event.Get<MetricEvent>(), res.Get<MetricEvent>());
        APSARA_TEST_FALSE(res.IsFromEventPool());
        APSARA_TEST_EQUAL(nullptr, res.GetEventPool());
    }
    {
        auto& event = mEventGroup->MutableEvents()[2];
        event->SetTimestamp(12345678901);
        auto res = event.Copy();
        APSARA_TEST_NOT_EQUAL(event.Get<SpanEvent>(), res.Get<SpanEvent>());
        APSARA_TEST_FALSE(res.IsFromEventPool());
        APSARA_TEST_EQUAL(nullptr, res.GetEventPool());
    }
    {
        auto& event = mEventGroup->MutableEvents()[3];
        event->SetTimestamp(12345678901);
        auto res = event.Copy();
        APSARA_TEST_NOT_EQUAL(event.Get<RawEvent>(), res.Get<RawEvent>());
        APSARA_TEST_FALSE(res.IsFromEventPool());
        APSARA_TEST_EQUAL(nullptr, res.GetEventPool());
    }
}

void PipelineEventPtrUnittest::TestDestruction() {
    { auto e = PipelineEventPtr(mEventGroup->CreateLogEvent(true).release(), true, nullptr); }
    APSARA_TEST_EQUAL(1U, gThreadedEventPool.mLogEventPool.size());
    { auto e = PipelineEventPtr(mEventGroup->CreateMetricEvent(true).release(), true, nullptr); }
    APSARA_TEST_EQUAL(1U, gThreadedEventPool.mMetricEventPool.size());
    { auto e = PipelineEventPtr(mEventGroup->CreateSpanEvent(true).release(), true, nullptr); }
    APSARA_TEST_EQUAL(1U, gThreadedEventPool.mSpanEventPool.size());
    { auto e = PipelineEventPtr(mEventGroup->CreateRawEvent(true).release(), true, nullptr); }
    APSARA_TEST_EQUAL(1U, gThreadedEventPool.mRawEventPool.size());

    EventPool pool(true);
    { auto e = PipelineEventPtr(mEventGroup->CreateLogEvent(true, &pool).release(), true, &pool); }
    APSARA_TEST_EQUAL(1U, pool.mLogEventPoolBak.size());
    { auto e = PipelineEventPtr(mEventGroup->CreateMetricEvent(true, &pool).release(), true, &pool); }
    APSARA_TEST_EQUAL(1U, pool.mMetricEventPoolBak.size());
    { auto e = PipelineEventPtr(mEventGroup->CreateSpanEvent(true, &pool).release(), true, &pool); }
    APSARA_TEST_EQUAL(1U, pool.mSpanEventPoolBak.size());
    { auto e = PipelineEventPtr(mEventGroup->CreateRawEvent(true, &pool).release(), true, &pool); }
    APSARA_TEST_EQUAL(1U, pool.mRawEventPoolBak.size());
}

void PipelineEventPtrUnittest::TestAssignment() {
    {
        auto e1 = PipelineEventPtr(mEventGroup->CreateLogEvent(true).release(), true, nullptr);
        auto e2 = PipelineEventPtr(mEventGroup->CreateLogEvent(true).release(), true, nullptr);
        e1 = std::move(e2);
        APSARA_TEST_EQUAL(1U, gThreadedEventPool.mLogEventPool.size());
    }
    {
        auto e1 = PipelineEventPtr(mEventGroup->CreateMetricEvent(true).release(), true, nullptr);
        auto e2 = PipelineEventPtr(mEventGroup->CreateMetricEvent(true).release(), true, nullptr);
        e1 = std::move(e2);
        APSARA_TEST_EQUAL(1U, gThreadedEventPool.mMetricEventPool.size());
    }
    {
        auto e1 = PipelineEventPtr(mEventGroup->CreateSpanEvent(true).release(), true, nullptr);
        auto e2 = PipelineEventPtr(mEventGroup->CreateSpanEvent(true).release(), true, nullptr);
        e1 = std::move(e2);
        APSARA_TEST_EQUAL(1U, gThreadedEventPool.mSpanEventPool.size());
    }
    {
        auto e1 = PipelineEventPtr(mEventGroup->CreateRawEvent(true).release(), true, nullptr);
        auto e2 = PipelineEventPtr(mEventGroup->CreateRawEvent(true).release(), true, nullptr);
        e1 = std::move(e2);
        APSARA_TEST_EQUAL(1U, gThreadedEventPool.mRawEventPool.size());
    }
    EventPool pool(true);
    {
        auto e1 = PipelineEventPtr(mEventGroup->CreateLogEvent(true, &pool).release(), true, &pool);
        auto e2 = PipelineEventPtr(mEventGroup->CreateLogEvent(true, &pool).release(), true, &pool);
        e1 = std::move(e2);
        APSARA_TEST_EQUAL(1U, pool.mLogEventPoolBak.size());
    }
    {
        auto e1 = PipelineEventPtr(mEventGroup->CreateMetricEvent(true, &pool).release(), true, &pool);
        auto e2 = PipelineEventPtr(mEventGroup->CreateMetricEvent(true, &pool).release(), true, &pool);
        e1 = std::move(e2);
        APSARA_TEST_EQUAL(1U, pool.mMetricEventPoolBak.size());
    }
    {
        auto e1 = PipelineEventPtr(mEventGroup->CreateSpanEvent(true, &pool).release(), true, &pool);
        auto e2 = PipelineEventPtr(mEventGroup->CreateSpanEvent(true, &pool).release(), true, &pool);
        e1 = std::move(e2);
        APSARA_TEST_EQUAL(1U, pool.mSpanEventPoolBak.size());
    }
    {
        auto e1 = PipelineEventPtr(mEventGroup->CreateRawEvent(true, &pool).release(), true, &pool);
        auto e2 = PipelineEventPtr(mEventGroup->CreateRawEvent(true, &pool).release(), true, &pool);
        e1 = std::move(e2);
        APSARA_TEST_EQUAL(1U, pool.mRawEventPoolBak.size());
    }
}

UNIT_TEST_CASE(PipelineEventPtrUnittest, TestIs)
UNIT_TEST_CASE(PipelineEventPtrUnittest, TestGet)
UNIT_TEST_CASE(PipelineEventPtrUnittest, TestCast)
UNIT_TEST_CASE(PipelineEventPtrUnittest, TestRelease)
UNIT_TEST_CASE(PipelineEventPtrUnittest, TestCopy)
UNIT_TEST_CASE(PipelineEventPtrUnittest, TestDestruction)
UNIT_TEST_CASE(PipelineEventPtrUnittest, TestAssignment)

} // namespace logtail

UNIT_TEST_MAIN
