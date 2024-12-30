#pragma once

#include <memory>
#include <string>

#include "Labels.h"
#include "models/PipelineEventGroup.h"
#include "pipeline/queue/QueueKey.h"

#ifdef APSARA_UNIT_TEST_MAIN
#include <vector>

#include "pipeline/queue/ProcessQueueItem.h"
#endif

namespace logtail::prom {
class StreamScraper {
public:
    StreamScraper(Labels labels, QueueKey queueKey, size_t inputIndex)
        : mEventGroup(PipelineEventGroup(std::make_shared<SourceBuffer>())),
          mQueueKey(queueKey),
          mInputIndex(inputIndex),
          mTargetLabels(std::move(labels)) {}

    static size_t MetricWriteCallback(char* buffer, size_t size, size_t nmemb, void* data);
    void FlushCache();
    void SendMetrics();
    void Reset();
    void SetAutoMetricMeta(double scrapeDurationSeconds, bool upState, const std::string& scrapeState);

    void SetScrapeTime(std::chrono::system_clock::time_point scrapeTime);

    std::string mHash;
    size_t mRawSize = 0;
    uint64_t mStreamIndex = 0;
    uint64_t mScrapeSamplesScraped = 0;
    EventPool* mEventPool = nullptr;

private:
    void AddEvent(const char* line, size_t len);
    void PushEventGroup(PipelineEventGroup&&) const;
    void SetTargetLabels(PipelineEventGroup& eGroup) const;
    std::string GetId();

    size_t mCurrStreamSize = 0;
    std::string mCache;
    PipelineEventGroup mEventGroup;

    // pipeline
    QueueKey mQueueKey;
    size_t mInputIndex;

    Labels mTargetLabels;

    // auto metrics
    uint64_t mScrapeTimestampMilliSec = 0;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorParsePrometheusMetricUnittest;
    friend class ScrapeSchedulerUnittest;
    friend class StreamScraperUnittest;
    mutable std::vector<std::shared_ptr<ProcessQueueItem>> mItem;
#endif
};
} // namespace logtail::prom
