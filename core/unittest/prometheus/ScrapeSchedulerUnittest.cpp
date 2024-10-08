/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <memory>
#include <string>

#include "common/StringTools.h"
#include "common/timer/Timer.h"
#include "prometheus/Constants.h"
#include "prometheus/async/PromFuture.h"
#include "prometheus/labels/Labels.h"
#include "prometheus/schedulers/ScrapeConfig.h"
#include "prometheus/schedulers/ScrapeScheduler.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class MockTimer : public Timer {
public:
    void Init() {}
    void PushEvent(std::unique_ptr<TimerEvent>&& e) { mQueue.push_back(std::move(e)); }
    void Stop() {}
    std::vector<std::unique_ptr<TimerEvent>> mQueue;
};

class ScrapeSchedulerUnittest : public testing::Test {
public:
    void TestInitscrapeScheduler();
    void TestProcess();
    void TestSplitByLines();
    void TestReceiveMessage();

    void TestScheduler();

protected:
    void SetUp() override {
        mScrapeConfig = make_shared<ScrapeConfig>();
        mScrapeConfig->mJobName = "test_job";
        mScrapeConfig->mScheme = "http";
        mScrapeConfig->mScrapeIntervalSeconds = 10;
        mScrapeConfig->mScrapeTimeoutSeconds = 10;
        mScrapeConfig->mMetricsPath = "/metrics";
        mScrapeConfig->mRequestHeaders = {{"Authorization", "Bearer xxxxx"}};

        mHttpResponse.mBody
            = "# HELP go_gc_duration_seconds A summary of the pause duration of garbage collection cycles.\n"
              "# TYPE go_gc_duration_seconds summary\n"
              "go_gc_duration_seconds{quantile=\"0\"} 1.5531e-05\n"
              "go_gc_duration_seconds{quantile=\"0.25\"} 3.9357e-05\n"
              "go_gc_duration_seconds{quantile=\"0.5\"} 4.1114e-05\n"
              "go_gc_duration_seconds{quantile=\"0.75\"} 4.3372e-05\n"
              "go_gc_duration_seconds{quantile=\"1\"} 0.000112326\n"
              "go_gc_duration_seconds_sum 0.034885631\n"
              "go_gc_duration_seconds_count 850\n"
              "# HELP go_goroutines Number of goroutines that currently exist.\n"
              "# TYPE go_goroutines gauge\n"
              "go_goroutines 7\n"
              "# HELP go_info Information about the Go environment.\n"
              "# TYPE go_info gauge\n"
              "go_info{version=\"go1.22.3\"} 1\n"
              "# HELP go_memstats_alloc_bytes Number of bytes allocated and still in use.\n"
              "# TYPE go_memstats_alloc_bytes gauge\n"
              "go_memstats_alloc_bytes 6.742688e+06\n"
              "# HELP go_memstats_alloc_bytes_total Total number of bytes allocated, even if freed.\n"
              "# TYPE go_memstats_alloc_bytes_total counter\n"
              "go_memstats_alloc_bytes_total 1.5159292e+08";

        mHttpResponse.mStatusCode = 200;
    }

private:
    std::shared_ptr<ScrapeConfig> mScrapeConfig;
    HttpResponse mHttpResponse;
};

void ScrapeSchedulerUnittest::TestInitscrapeScheduler() {
    Labels labels;
    labels.Set(prometheus::ADDRESS_LABEL_NAME, "localhost:8080");
    ScrapeScheduler event(mScrapeConfig, "localhost", 8080, labels, 0, 0);
    APSARA_TEST_EQUAL(event.GetId(), "test_jobhttp://localhost:8080/metrics" + ToString(labels.Hash()));
}

void ScrapeSchedulerUnittest::TestProcess() {
    Labels labels;
    labels.Set(prometheus::ADDRESS_LABEL_NAME, "localhost:8080");
    labels.Set(prometheus::ADDRESS_LABEL_NAME, "localhost:8080");
    ScrapeScheduler event(mScrapeConfig, "localhost", 8080, labels, 0, 0);
    APSARA_TEST_EQUAL(event.GetId(), "test_jobhttp://localhost:8080/metrics" + ToString(labels.Hash()));
    // if status code is not 200, no data will be processed
    // but will continue running, sending self-monitoring metrics
    mHttpResponse.mStatusCode = 503;
    event.OnMetricResult(mHttpResponse, 0);
    APSARA_TEST_EQUAL(1UL, event.mItem.size());
    event.mItem.clear();

    mHttpResponse.mStatusCode = 200;
    event.OnMetricResult(mHttpResponse, 0);
    APSARA_TEST_EQUAL(1UL, event.mItem.size());
    APSARA_TEST_EQUAL(11UL, event.mItem[0]->mEventGroup.GetEvents().size());
}

void ScrapeSchedulerUnittest::TestSplitByLines() {
    Labels labels;
    labels.Set(prometheus::ADDRESS_LABEL_NAME, "localhost:8080");
    labels.Set(prometheus::ADDRESS_LABEL_NAME, "localhost:8080");
    ScrapeScheduler event(mScrapeConfig, "localhost", 8080, labels, 0, 0);
    APSARA_TEST_EQUAL(event.GetId(), "test_jobhttp://localhost:8080/metrics" + ToString(labels.Hash()));
    auto res = event.BuildPipelineEventGroup(mHttpResponse.mBody);
    APSARA_TEST_EQUAL(11UL, res.GetEvents().size());
    APSARA_TEST_EQUAL("go_gc_duration_seconds{quantile=\"0\"} 1.5531e-05",
                      res.GetEvents()[0].Cast<LogEvent>().GetContent(prometheus::PROMETHEUS).to_string());
    APSARA_TEST_EQUAL("go_gc_duration_seconds{quantile=\"0.25\"} 3.9357e-05",
                      res.GetEvents()[1].Cast<LogEvent>().GetContent(prometheus::PROMETHEUS).to_string());
    APSARA_TEST_EQUAL("go_gc_duration_seconds{quantile=\"0.5\"} 4.1114e-05",
                      res.GetEvents()[2].Cast<LogEvent>().GetContent(prometheus::PROMETHEUS).to_string());
    APSARA_TEST_EQUAL("go_gc_duration_seconds{quantile=\"0.75\"} 4.3372e-05",
                      res.GetEvents()[3].Cast<LogEvent>().GetContent(prometheus::PROMETHEUS).to_string());
    APSARA_TEST_EQUAL("go_gc_duration_seconds{quantile=\"1\"} 0.000112326",
                      res.GetEvents()[4].Cast<LogEvent>().GetContent(prometheus::PROMETHEUS).to_string());
    APSARA_TEST_EQUAL("go_gc_duration_seconds_sum 0.034885631",
                      res.GetEvents()[5].Cast<LogEvent>().GetContent(prometheus::PROMETHEUS).to_string());
    APSARA_TEST_EQUAL("go_gc_duration_seconds_count 850",
                      res.GetEvents()[6].Cast<LogEvent>().GetContent(prometheus::PROMETHEUS).to_string());
    APSARA_TEST_EQUAL("go_goroutines 7",
                      res.GetEvents()[7].Cast<LogEvent>().GetContent(prometheus::PROMETHEUS).to_string());
    APSARA_TEST_EQUAL("go_info{version=\"go1.22.3\"} 1",
                      res.GetEvents()[8].Cast<LogEvent>().GetContent(prometheus::PROMETHEUS).to_string());
    APSARA_TEST_EQUAL("go_memstats_alloc_bytes 6.742688e+06",
                      res.GetEvents()[9].Cast<LogEvent>().GetContent(prometheus::PROMETHEUS).to_string());
    APSARA_TEST_EQUAL("go_memstats_alloc_bytes_total 1.5159292e+08",
                      res.GetEvents()[10].Cast<LogEvent>().GetContent(prometheus::PROMETHEUS).to_string());
}

void ScrapeSchedulerUnittest::TestReceiveMessage() {
    Labels labels;
    labels.Set(prometheus::ADDRESS_LABEL_NAME, "localhost:8080");
    labels.Set(prometheus::ADDRESS_LABEL_NAME, "localhost:8080");
    auto event = make_shared<ScrapeScheduler>(mScrapeConfig, "localhost", 8080, labels, 0, 0);


    // before
    APSARA_TEST_EQUAL(true, event->IsCancelled());


    // after
    APSARA_TEST_EQUAL(false, event->IsCancelled());
}

void ScrapeSchedulerUnittest::TestScheduler() {
    Labels labels;
    labels.Set(prometheus::ADDRESS_LABEL_NAME, "localhost:8080");
    ScrapeScheduler event(mScrapeConfig, "localhost", 8080, labels, 0, 0);
    auto timer = make_shared<MockTimer>();
    event.SetTimer(timer);
    event.ScheduleNext();

    APSARA_TEST_TRUE(timer->mQueue.size() == 1);

    event.Cancel();

    APSARA_TEST_TRUE(event.mValidState == false);
    APSARA_TEST_TRUE(event.mFuture->mState == PromFutureState::Done);
}

UNIT_TEST_CASE(ScrapeSchedulerUnittest, TestInitscrapeScheduler)
UNIT_TEST_CASE(ScrapeSchedulerUnittest, TestProcess)
UNIT_TEST_CASE(ScrapeSchedulerUnittest, TestSplitByLines)

} // namespace logtail

UNIT_TEST_MAIN