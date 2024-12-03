#include "plugin/processor/inner/ProcessorPromParseMetricNative.h"

#include <json/json.h>

#include "common/StringTools.h"
#include "models/MetricEvent.h"
#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"
#include "models/RawEvent.h"
#include "prometheus/Constants.h"

using namespace std;
namespace logtail {

const string ProcessorPromParseMetricNative::sName = "processor_prom_parse_metric_native";

// only for inner processor
bool ProcessorPromParseMetricNative::Init(const Json::Value& config) {
    mScrapeConfigPtr = std::make_unique<ScrapeConfig>();
    if (!mScrapeConfigPtr->InitStaticConfig(config)) {
        return false;
    }
    return true;
}

void ProcessorPromParseMetricNative::Process(PipelineEventGroup& eGroup) {
    if (!eGroup.HasMetadata(EventGroupMetaKey::PROMETHEUS_STREAM_ID)) {
        LOG_WARNING(sLogger, ("unexpected event", "need prom stream id"));
        return;
    }

    EventsContainer& events = eGroup.MutableEvents();
    auto rawEvents = std::move(events);
    events.reserve(rawEvents.size());

    StringView scrapeTimestampMilliSecStr = eGroup.GetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_TIMESTAMP_MILLISEC);
    auto timestampMilliSec = StringTo<uint64_t>(scrapeTimestampMilliSecStr.to_string());
    auto timestamp = timestampMilliSec / 1000;
    auto nanoSec = timestampMilliSec % 1000 * 1000000;
    TextParser parser(mScrapeConfigPtr->mHonorTimestamps);
    parser.SetDefaultTimestamp(timestamp, nanoSec);

    for (auto& rawEvent : rawEvents) {
        ProcessEvent(rawEvent, events, eGroup, parser);
    }
}

bool ProcessorPromParseMetricNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<RawEvent>();
}

bool ProcessorPromParseMetricNative::ProcessEvent(PipelineEventPtr& e,
                                                  EventsContainer& events,
                                                  PipelineEventGroup& eGroup,
                                                  TextParser& parser) {
    if (!e.Is<RawEvent>()) {
        LOG_WARNING(sLogger, ("unexpected event type", "need raw event"));
        return false;
    }
    auto rawEvent = e.Cast<RawEvent>();
    auto content = rawEvent.GetContent();
    if (content.empty()) {
        LOG_WARNING(sLogger, ("empty content", ""));
        return false;
    }
    auto metricEvent = eGroup.CreateMetricEvent(true);
    if (parser.ParseLine(content, *metricEvent)) {
        metricEvent->SetTagNoCopy(prometheus::NAME, metricEvent->GetName());
        events.emplace_back(std::move(metricEvent), true, nullptr);
        return true;
    }
    return false;
}

} // namespace logtail
