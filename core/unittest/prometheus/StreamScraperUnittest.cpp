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

#include "EventPool.h"
#include "Flags.h"
#include "models/RawEvent.h"
#include "prometheus/Constants.h"
#include "prometheus/component/StreamScraper.h"
#include "prometheus/labels/Labels.h"
#include "prometheus/schedulers/ScrapeConfig.h"
#include "unittest/Unittest.h"

using namespace std;

DECLARE_FLAG_INT64(prom_stream_bytes_size);

namespace logtail::prom {
class StreamScraperUnittest : public testing::Test {
public:
    void TestStreamMetricWriteCallback();
    void TestStreamSendMetric();


protected:
    void SetUp() override {
        mScrapeConfig = make_shared<ScrapeConfig>();
        mScrapeConfig->mJobName = "test_job";
        mScrapeConfig->mScheme = "http";
        mScrapeConfig->mScrapeIntervalSeconds = 10;
        mScrapeConfig->mScrapeTimeoutSeconds = 10;
        mScrapeConfig->mMetricsPath = "/metrics";
        mScrapeConfig->mRequestHeaders = {{"Authorization", "Bearer xxxxx"}};
    }

private:
    std::shared_ptr<ScrapeConfig> mScrapeConfig;
};

void StreamScraperUnittest::TestStreamMetricWriteCallback() {
    EventPool eventPool{true};

    Labels labels;
    labels.Set(prometheus::ADDRESS_LABEL_NAME, "localhost:8080");
    auto streamScraper = make_shared<StreamScraper>(labels, 0, 0);
    streamScraper->mEventPool = &eventPool;
    streamScraper->mHash = "test_jobhttp://localhost:8080/metrics";

    string body1 = "# HELP go_gc_duration_seconds A summary of the pause duration of garbage collection cycles.\n"
                   "# TYPE go_gc_duration_seconds summary\n"
                   "go_gc_duration_seconds{quantile=\"0\"} 1.5531e-05\n"
                   "go_gc_duration_seconds{quantile=\"0.25\"} 3.9357e-05\n"
                   "go_gc_duration_seconds{quantile=\"0.5\"} 4.1114e-05\n"
                   "go_gc_duration_seconds{quantile=\"0.75\"} 4.3372e-05\n"
                   "go_gc_duration_seconds{quantile=\"1\"} 0.000112326\n"
                   "go_gc_duration_seconds_sum 0.034885631\n"
                   "go_gc_duration_seconds_count 850\n"
                   "# HELP go_goroutines Number of goroutines t"
                   "hat currently exist.\n"
                   "# TYPE go_goroutines gauge\n"
                   "go_go";
    string body2 = "routines 7\n"
                   "# HELP go_info Information about the Go environment.\n"
                   "# TYPE go_info gauge\n"
                   "go_info{version=\"go1.22.3\"} 1\n"
                   "# HELP go_memstats_alloc_bytes Number of bytes allocated and still in use.\n"
                   "# TYPE go_memstats_alloc_bytes gauge\n"
                   "go_memstats_alloc_bytes 6.742688e+06\n"
                   "# HELP go_memstats_alloc_bytes_total Total number of bytes allocated, even if freed.\n"
                   "# TYPE go_memstats_alloc_bytes_total counter\n"
                   "go_memstats_alloc_bytes_total 1.5159292e+08";

    StreamScraper::MetricWriteCallback(body1.data(), (size_t)1, (size_t)body1.length(), streamScraper.get());
    auto& res = streamScraper->mEventGroup;
    APSARA_TEST_EQUAL(7UL, res.GetEvents().size());
    APSARA_TEST_EQUAL("go_gc_duration_seconds{quantile=\"0\"} 1.5531e-05",
                      res.GetEvents()[0].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_gc_duration_seconds{quantile=\"0.25\"} 3.9357e-05",
                      res.GetEvents()[1].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_gc_duration_seconds{quantile=\"0.5\"} 4.1114e-05",
                      res.GetEvents()[2].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_gc_duration_seconds{quantile=\"0.75\"} 4.3372e-05",
                      res.GetEvents()[3].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_gc_duration_seconds{quantile=\"1\"} 0.000112326",
                      res.GetEvents()[4].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_gc_duration_seconds_sum 0.034885631", res.GetEvents()[5].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_gc_duration_seconds_count 850", res.GetEvents()[6].Cast<RawEvent>().GetContent());

    StreamScraper::MetricWriteCallback(body2.data(), (size_t)1, (size_t)body2.length(), streamScraper.get());
    streamScraper->FlushCache();
    APSARA_TEST_EQUAL(11UL, res.GetEvents().size());

    APSARA_TEST_EQUAL("go_goroutines 7", res.GetEvents()[7].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_info{version=\"go1.22.3\"} 1", res.GetEvents()[8].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_memstats_alloc_bytes 6.742688e+06", res.GetEvents()[9].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_memstats_alloc_bytes_total 1.5159292e+08", res.GetEvents()[10].Cast<RawEvent>().GetContent());
}

void StreamScraperUnittest::TestStreamSendMetric() {
    EventPool eventPool{true};

    Labels labels;
    labels.Set(prometheus::ADDRESS_LABEL_NAME, "localhost:8080");
    auto streamScraper = make_shared<StreamScraper>(labels, 0, 0);
    streamScraper->mEventPool = &eventPool;
    streamScraper->mHash = "test_jobhttp://localhost:8080/metrics";

    string body1 = "# HELP go_gc_duration_seconds A summary of the pause duration of garbage collection cycles.\n"
                   "# TYPE go_gc_duration_seconds summary\n"
                   "go_gc_duration_seconds{quantile=\"0\"} 1.5531e-05\n"
                   "go_gc_duration_seconds{quantile=\"0.25\"} 3.9357e-05\n"
                   "go_gc_duration_seconds{quantile=\"0.5\"} 4.1114e-05\n"
                   "go_gc_duration_seconds{quantile=\"0.75\"} 4.3372e-05\n"
                   "go_gc_duration_seconds{quantile=\"1\"} 0.000112326\n"
                   "go_gc_duration_seconds_sum 0.034885631\n"
                   "go_gc_duration_seconds_count 850\n"
                   "# HELP go_goroutines Number of goroutines t"
                   "hat currently exist.\n"
                   "# TYPE go_goroutines gauge\n"
                   "go_go";
    string body2 = "routines 7\n"
                   "# HELP go_info Information about the Go environment.\n"
                   "# TYPE go_info gauge\n"
                   "go_info{version=\"go1.22.3\"} 1\n"
                   "# HELP go_memstats_alloc_bytes Number of bytes allocated and still in use.\n"
                   "# TYPE go_memstats_alloc_bytes gauge\n"
                   "go_memstats_alloc_bytes 6.742688e+06\n"
                   "# HELP go_memstats_alloc_bytes_total Total number of bytes allocated, even if freed.\n"
                   "# TYPE go_memstats_alloc_bytes_total counter\n"
                   "go_memstats_alloc_bytes_total 1.5159292e+08";

    INT64_FLAG(prom_stream_bytes_size) = body1.length() - 2;

    StreamScraper::MetricWriteCallback(body1.data(), (size_t)1, (size_t)body1.length(), streamScraper.get());
    APSARA_TEST_EQUAL(0UL, streamScraper->mEventGroup.GetEvents().size());

    auto& res = streamScraper->mItem[0]->mEventGroup;
    APSARA_TEST_EQUAL(7UL, res.GetEvents().size());
    APSARA_TEST_EQUAL("go_gc_duration_seconds{quantile=\"0\"} 1.5531e-05",
                      res.GetEvents()[0].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_gc_duration_seconds{quantile=\"0.25\"} 3.9357e-05",
                      res.GetEvents()[1].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_gc_duration_seconds{quantile=\"0.5\"} 4.1114e-05",
                      res.GetEvents()[2].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_gc_duration_seconds{quantile=\"0.75\"} 4.3372e-05",
                      res.GetEvents()[3].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_gc_duration_seconds{quantile=\"1\"} 0.000112326",
                      res.GetEvents()[4].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_gc_duration_seconds_sum 0.034885631", res.GetEvents()[5].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_gc_duration_seconds_count 850", res.GetEvents()[6].Cast<RawEvent>().GetContent());

    StreamScraper::MetricWriteCallback(body2.data(), (size_t)1, (size_t)body2.length(), streamScraper.get());
    APSARA_TEST_EQUAL(3UL, streamScraper->mEventGroup.GetEvents().size());
    streamScraper->FlushCache();
    APSARA_TEST_EQUAL(4UL, streamScraper->mEventGroup.GetEvents().size());
    streamScraper->SendMetrics();
    auto& res1 = streamScraper->mItem[1]->mEventGroup;
    APSARA_TEST_EQUAL(4UL, res1.GetEvents().size());

    APSARA_TEST_EQUAL("go_goroutines 7", res1.GetEvents()[0].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_info{version=\"go1.22.3\"} 1", res1.GetEvents()[1].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_memstats_alloc_bytes 6.742688e+06", res1.GetEvents()[2].Cast<RawEvent>().GetContent());
    APSARA_TEST_EQUAL("go_memstats_alloc_bytes_total 1.5159292e+08", res1.GetEvents()[3].Cast<RawEvent>().GetContent());
}


UNIT_TEST_CASE(StreamScraperUnittest, TestStreamMetricWriteCallback)
UNIT_TEST_CASE(StreamScraperUnittest, TestStreamSendMetric)


} // namespace logtail::prom

UNIT_TEST_MAIN