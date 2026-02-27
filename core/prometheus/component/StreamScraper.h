#pragma once

#include <memory>
#include <string>
#include <utility>

#include "Labels.h"
#include "collection_pipeline/queue/QueueKey.h"
#include "models/PipelineEventGroup.h"

#ifdef APSARA_UNIT_TEST_MAIN
#include <vector>

#include "collection_pipeline/queue/ProcessQueueItem.h"
#endif

namespace logtail::prom {
class StreamScraper {
public:
    StreamScraper(Labels labels,
                  QueueKey queueKey,
                  size_t inputIndex,
                  std::string hash,
                  EventPool* eventPool,
                  std::chrono::system_clock::time_point scrapeTime);
    static size_t MetricWriteCallback(char* buffer, size_t size, size_t nmemb, void* data);
    void FlushCache();
    void SendMetrics();
    void Reset();
    void SetAutoMetricMeta(double scrapeDurationSeconds, bool upState, const std::string& scrapeState);

    size_t mRawSize = 0;
    static size_t mMaxSampleLength;
    uint64_t mStreamIndex = 0;

private:
    void AddEvent(const char* line, size_t len);
    void PushEventGroup(PipelineEventGroup&&) const;
    void SetTargetLabels(PipelineEventGroup& eGroup) const;
    std::string GetId();

    size_t mCurrStreamSize = 0;
    std::string mCache;
    PipelineEventGroup mEventGroup;

    std::string mHash;
    uint64_t mScrapeSamplesScraped = 0;
    EventPool* mEventPool = nullptr;

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
