// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "models/EventPool.h"
#include "pipeline/batch/BatchedEvents.h"
#include "runner/ProcessorRunner.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class BatchedEventsUnittest : public ::testing::Test {
public:
    void TestDestructor();

protected:
    void TearDown() override {
        mPool.Clear();
        gThreadedEventPool.Clear();
    }

private:
    EventPool mPool;
};

void BatchedEventsUnittest::TestDestructor() {
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

UNIT_TEST_CASE(BatchedEventsUnittest, TestDestructor)

} // namespace logtail

UNIT_TEST_MAIN
