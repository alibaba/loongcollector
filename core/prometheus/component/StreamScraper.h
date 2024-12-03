#pragma once

#include <memory>
#include <string>

#include "Labels.h"
#include "models/PipelineEventGroup.h"
#include "pipeline/queue/QueueKey.h"


namespace logtail::prom {
class PromStreamScraper {
public:
    PromStreamScraper(Labels labels, QueueKey queueKey, size_t inputIndex)
        : mEventGroup(PipelineEventGroup(std::make_shared<SourceBuffer>())),
          mQueueKey(queueKey),
          mInputIndex(inputIndex),
          mTargetLabels(std::move(labels)) {}

    static size_t MetricWriteCallback(char* buffer, size_t size, size_t nmemb, void* data);
    void FlushCache();
    void SendMetrics();
    void Reset();
    void SetAutoMetricMeta(double scrapeDurationSeconds, bool upState);

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

};
} // namespace logtail::prom
