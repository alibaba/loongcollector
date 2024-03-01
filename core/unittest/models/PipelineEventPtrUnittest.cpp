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

#include "models/PipelineEventPtr.h"
#include "unittest/Unittest.h"

namespace logtail {

class PipelineEventPtrUnittest : public ::testing::Test {
public:
    void TestIs();
    void TestGet();
    void TestCast();

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
    PipelineEventPtr logEventPtr(mEventGroup->CreateLogEvent());
    PipelineEventPtr metricEventPtr(mEventGroup->CreateMetricEvent());
    PipelineEventPtr spanEventPtr(mEventGroup->CreateSpanEvent());
    APSARA_TEST_TRUE_FATAL(logEventPtr.Is<LogEvent>());
    APSARA_TEST_FALSE_FATAL(logEventPtr.Is<MetricEvent>());
    APSARA_TEST_FALSE_FATAL(logEventPtr.Is<SpanEvent>());
    APSARA_TEST_FALSE_FATAL(metricEventPtr.Is<LogEvent>());
    APSARA_TEST_TRUE_FATAL(metricEventPtr.Is<MetricEvent>());
    APSARA_TEST_FALSE_FATAL(metricEventPtr.Is<SpanEvent>());
    APSARA_TEST_FALSE_FATAL(spanEventPtr.Is<LogEvent>());
    APSARA_TEST_FALSE_FATAL(spanEventPtr.Is<MetricEvent>());
    APSARA_TEST_TRUE_FATAL(spanEventPtr.Is<SpanEvent>());
}

void PipelineEventPtrUnittest::TestGet() {
    auto logUPtr = mEventGroup->CreateLogEvent();
    auto logAddr = logUPtr.get();
    PipelineEventPtr logEventPtr(mEventGroup->CreateLogEvent());
    LogEvent* log = logEventPtr.Get<LogEvent>();
    APSARA_TEST_EQUAL_FATAL(logAddr, log);
}

void PipelineEventPtrUnittest::TestCast() {
    auto logUPtr = mEventGroup->CreateLogEvent();
    auto logAddr = logUPtr.get();
    PipelineEventPtr logEventPtr(mEventGroup->CreateLogEvent());
    LogEvent& log = logEventPtr.Cast<LogEvent>();
    APSARA_TEST_EQUAL_FATAL(logAddr, &log);
}

UNIT_TEST_CASE(PipelineEventPtrUnittest, TestIs)

} // namespace logtail

UNIT_TEST_MAIN
