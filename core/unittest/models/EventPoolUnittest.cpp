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

#include "models/EventPool.h"
#include "models/PipelineEventGroup.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class EventPoolUnittest : public ::testing::Test {
public:
    void TestNoLock();
    void TestLock();
    void TestGC();

protected:
    void SetUp() override { mGroup.reset(new PipelineEventGroup(make_shared<SourceBuffer>())); }

private:
    unique_ptr<PipelineEventGroup> mGroup;
};

void EventPoolUnittest::TestNoLock() {
    EventPool pool(false);
    APSARA_TEST_FALSE(pool.mEnableLock);
    {
        auto e = pool.AcquireLogEvent(mGroup.get());
        pool.Release({e});
        APSARA_TEST_EQUAL(1U, pool.mLogEventPool.size());
        APSARA_TEST_EQUAL(e, pool.mLogEventPool[0]);
        APSARA_TEST_EQUAL(mGroup.get(), e->GetPipelineEventGroupPtr());

        PipelineEventGroup g(make_shared<SourceBuffer>());
        e = pool.AcquireLogEvent(&g);
        APSARA_TEST_EQUAL(0U, pool.mLogEventPool.size());
        APSARA_TEST_EQUAL(0U, pool.mMinUnusedLogEventsCnt);
        APSARA_TEST_EQUAL(&g, e->GetPipelineEventGroupPtr());
    }
    {
        auto e = pool.AcquireMetricEvent(mGroup.get());
        pool.Release({e});
        APSARA_TEST_EQUAL(1U, pool.mMetricEventPool.size());
        APSARA_TEST_EQUAL(e, pool.mMetricEventPool[0]);
        APSARA_TEST_EQUAL(mGroup.get(), e->GetPipelineEventGroupPtr());

        PipelineEventGroup g(make_shared<SourceBuffer>());
        e = pool.AcquireMetricEvent(&g);
        APSARA_TEST_EQUAL(0U, pool.mMetricEventPool.size());
        APSARA_TEST_EQUAL(0U, pool.mMinUnusedMetricEventsCnt);
        APSARA_TEST_EQUAL(&g, e->GetPipelineEventGroupPtr());
    }
    {
        auto e = pool.AcquireSpanEvent(mGroup.get());
        pool.Release({e});
        APSARA_TEST_EQUAL(1U, pool.mSpanEventPool.size());
        APSARA_TEST_EQUAL(e, pool.mSpanEventPool[0]);
        APSARA_TEST_EQUAL(mGroup.get(), e->GetPipelineEventGroupPtr());

        PipelineEventGroup g(make_shared<SourceBuffer>());
        e = pool.AcquireSpanEvent(&g);
        APSARA_TEST_EQUAL(0U, pool.mSpanEventPool.size());
        APSARA_TEST_EQUAL(0U, pool.mMinUnusedSpanEventsCnt);
        APSARA_TEST_EQUAL(&g, e->GetPipelineEventGroupPtr());
    }
}

void EventPoolUnittest::TestLock() {
    EventPool pool;
    APSARA_TEST_TRUE(pool.mEnableLock);
    {
        auto e = pool.AcquireLogEvent(mGroup.get());
        auto e1 = pool.AcquireLogEvent(mGroup.get());
        pool.Release({e});
        APSARA_TEST_EQUAL(1U, pool.mLogEventPoolBak.size());
        APSARA_TEST_EQUAL(e, pool.mLogEventPoolBak[0]);
        APSARA_TEST_EQUAL(mGroup.get(), e->GetPipelineEventGroupPtr());

        e = pool.AcquireLogEvent(mGroup.get());
        APSARA_TEST_EQUAL(0U, pool.mLogEventPoolBak.size());
        APSARA_TEST_EQUAL(0U, pool.mLogEventPool.size());
        APSARA_TEST_EQUAL(mGroup.get(), e->GetPipelineEventGroupPtr());

        pool.Release(vector<LogEvent*>{e, e1});
        pool.AcquireLogEvent(mGroup.get());
        APSARA_TEST_EQUAL(0U, pool.mLogEventPoolBak.size());
        APSARA_TEST_EQUAL(1U, pool.mLogEventPool.size());
    }
    {
        auto e = pool.AcquireMetricEvent(mGroup.get());
        auto e1 = pool.AcquireMetricEvent(mGroup.get());
        pool.Release({e});
        APSARA_TEST_EQUAL(1U, pool.mMetricEventPoolBak.size());
        APSARA_TEST_EQUAL(e, pool.mMetricEventPoolBak[0]);
        APSARA_TEST_EQUAL(mGroup.get(), e->GetPipelineEventGroupPtr());

        e = pool.AcquireMetricEvent(mGroup.get());
        APSARA_TEST_EQUAL(0U, pool.mMetricEventPoolBak.size());
        APSARA_TEST_EQUAL(0U, pool.mMetricEventPool.size());
        APSARA_TEST_EQUAL(mGroup.get(), e->GetPipelineEventGroupPtr());

        pool.Release(vector<MetricEvent*>{e, e1});
        pool.AcquireMetricEvent(mGroup.get());
        APSARA_TEST_EQUAL(0U, pool.mMetricEventPoolBak.size());
        APSARA_TEST_EQUAL(1U, pool.mMetricEventPool.size());
    }
    {
        auto e = pool.AcquireSpanEvent(mGroup.get());
        auto e1 = pool.AcquireSpanEvent(mGroup.get());
        pool.Release({e});
        APSARA_TEST_EQUAL(1U, pool.mSpanEventPoolBak.size());
        APSARA_TEST_EQUAL(e, pool.mSpanEventPoolBak[0]);
        APSARA_TEST_EQUAL(mGroup.get(), e->GetPipelineEventGroupPtr());

        e = pool.AcquireSpanEvent(mGroup.get());
        APSARA_TEST_EQUAL(0U, pool.mSpanEventPoolBak.size());
        APSARA_TEST_EQUAL(0U, pool.mSpanEventPool.size());
        APSARA_TEST_EQUAL(mGroup.get(), e->GetPipelineEventGroupPtr());

        pool.Release(vector<SpanEvent*>{e, e1});
        pool.AcquireSpanEvent(mGroup.get());
        APSARA_TEST_EQUAL(0U, pool.mSpanEventPoolBak.size());
        APSARA_TEST_EQUAL(1U, pool.mSpanEventPool.size());
    }
}

void EventPoolUnittest::TestGC() {
    {
        EventPool pool(false);

        vector<LogEvent*> events;
        for (size_t i = 0; i < 3; ++i) {
            events.push_back(pool.AcquireLogEvent(mGroup.get()));
        }

        pool.Release(std::move(events));
        pool.CheckGC();
        APSARA_TEST_EQUAL(0U, pool.mLogEventPool.size());
        APSARA_TEST_EQUAL(numeric_limits<size_t>::max(), pool.mMinUnusedLogEventsCnt);
    }
    {
        EventPool pool(false);

        vector<LogEvent*> events;
        for (size_t i = 0; i < 3; ++i) {
            events.push_back(pool.AcquireLogEvent(mGroup.get()));
        }

        pool.Release(std::move(events));
        auto e = pool.AcquireLogEvent(mGroup.get());
        pool.Release({e});
        pool.CheckGC();
        APSARA_TEST_EQUAL(1U, pool.mLogEventPool.size());
        APSARA_TEST_EQUAL(numeric_limits<size_t>::max(), pool.mMinUnusedLogEventsCnt);
    }
    {
        EventPool pool;

        vector<LogEvent*> events;
        for (size_t i = 0; i < 3; ++i) {
            events.push_back(pool.AcquireLogEvent(mGroup.get()));
        }

        pool.Release(std::move(events));
        pool.CheckGC();
        APSARA_TEST_EQUAL(0U, pool.mLogEventPool.size());
        APSARA_TEST_EQUAL(0U, pool.mLogEventPoolBak.size());
        APSARA_TEST_EQUAL(numeric_limits<size_t>::max(), pool.mMinUnusedLogEventsCnt);
    }
    {
        EventPool pool;

        vector<LogEvent*> events;
        for (size_t i = 0; i < 3; ++i) {
            events.push_back(pool.AcquireLogEvent(mGroup.get()));
        }

        pool.Release(std::move(events));
        auto e = pool.AcquireLogEvent(mGroup.get());
        pool.Release({e});
        pool.CheckGC();
        APSARA_TEST_EQUAL(0U, pool.mLogEventPool.size());
        APSARA_TEST_EQUAL(0U, pool.mLogEventPoolBak.size());
        APSARA_TEST_EQUAL(numeric_limits<size_t>::max(), pool.mMinUnusedLogEventsCnt);
    }
}

UNIT_TEST_CASE(EventPoolUnittest, TestNoLock)
UNIT_TEST_CASE(EventPoolUnittest, TestLock)
UNIT_TEST_CASE(EventPoolUnittest, TestGC)

} // namespace logtail

UNIT_TEST_MAIN
