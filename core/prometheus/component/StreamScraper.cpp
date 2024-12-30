#include "prometheus/component/StreamScraper.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "Flags.h"
#include "Labels.h"
#include "common/StringTools.h"
#include "models/PipelineEventGroup.h"
#include "pipeline/queue/ProcessQueueItem.h"
#include "pipeline/queue/ProcessQueueManager.h"
#include "prometheus/Utils.h"

DEFINE_FLAG_INT64(prom_stream_bytes_size, "stream bytes size", 1024 * 1024);

DEFINE_FLAG_BOOL(enable_prom_stream_scrape, "enable prom stream scrape", true);

using namespace std;

namespace logtail::prom {
size_t StreamScraper::MetricWriteCallback(char* buffer, size_t size, size_t nmemb, void* data) {
    uint64_t sizes = size * nmemb;

    if (buffer == nullptr || data == nullptr) {
        return 0;
    }

    auto* body = static_cast<StreamScraper*>(data);

    size_t begin = 0;
    for (size_t end = begin; end < sizes; ++end) {
        if (buffer[end] == '\n') {
            if (begin == 0 && !body->mCache.empty()) {
                body->mCache.append(buffer, end);
                body->AddEvent(body->mCache.data(), body->mCache.size());
                body->mCache.clear();
            } else if (begin != end) {
                body->AddEvent(buffer + begin, end - begin);
            }
            begin = end + 1;
        }
    }

    if (begin < sizes) {
        body->mCache.append(buffer + begin, sizes - begin);
    }
    body->mRawSize += sizes;
    body->mCurrStreamSize += sizes;

    if (BOOL_FLAG(enable_prom_stream_scrape) && body->mCurrStreamSize >= (size_t)INT64_FLAG(prom_stream_bytes_size)) {
        body->mStreamIndex++;
        body->SendMetrics();
    }

    return sizes;
}

void StreamScraper::AddEvent(const char* line, size_t len) {
    if (IsValidMetric(StringView(line, len))) {
        auto* e = mEventGroup.AddRawEvent(true, mEventPool);
        auto sb = mEventGroup.GetSourceBuffer()->CopyString(line, len);
        e->SetContentNoCopy(sb);
        mScrapeSamplesScraped++;
    }
}

void StreamScraper::FlushCache() {
    if (!mCache.empty()) {
        AddEvent(mCache.data(), mCache.size());
        mCache.clear();
    }
}

void StreamScraper::SetTargetLabels(PipelineEventGroup& eGroup) const {
    mTargetLabels.Range([&eGroup](const std::string& key, const std::string& value) { eGroup.SetTag(key, value); });
}

void StreamScraper::PushEventGroup(PipelineEventGroup&& eGroup) const {
    auto item = make_unique<ProcessQueueItem>(std::move(eGroup), mInputIndex);
#ifdef APSARA_UNIT_TEST_MAIN
    mItem.emplace_back(std::move(item));
    return;
#endif
    while (true) {
        if (ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item)) == 0) {
            break;
        }
        usleep(10 * 1000);
    }
}

void StreamScraper::SendMetrics() {
    mEventGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_TIMESTAMP_MILLISEC,
                            ToString(mScrapeTimestampMilliSec));
    mEventGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_STREAM_ID, GetId() + ToString(mScrapeTimestampMilliSec));

    SetTargetLabels(mEventGroup);
    PushEventGroup(std::move(mEventGroup));
    mEventGroup = PipelineEventGroup(std::make_shared<SourceBuffer>());
    mCurrStreamSize = 0;
}

void StreamScraper::Reset() {
    mEventGroup = PipelineEventGroup(std::make_shared<SourceBuffer>());
    mRawSize = 0;
    mCurrStreamSize = 0;
    mCache.clear();
    mStreamIndex = 0;
    mScrapeSamplesScraped = 0;
}

void StreamScraper::SetAutoMetricMeta(double scrapeDurationSeconds, bool upState, const string& scrapeState) {
    mEventGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_STATE, scrapeState);
    mEventGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_TIMESTAMP_MILLISEC,
                            ToString(mScrapeTimestampMilliSec));
    mEventGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_SAMPLES_SCRAPED, ToString(mScrapeSamplesScraped));
    mEventGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_DURATION, ToString(scrapeDurationSeconds));
    mEventGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_RESPONSE_SIZE, ToString(mRawSize));
    mEventGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_UP_STATE, ToString(upState));
    mEventGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_STREAM_ID, GetId() + ToString(mScrapeTimestampMilliSec));
    mEventGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_STREAM_TOTAL, ToString(mStreamIndex));
}
std::string StreamScraper::GetId() {
    return mHash;
}
void StreamScraper::SetScrapeTime(std::chrono::system_clock::time_point scrapeTime) {
    mScrapeTimestampMilliSec
        = std::chrono::duration_cast<std::chrono::milliseconds>(scrapeTime.time_since_epoch()).count();
}
} // namespace logtail::prom
