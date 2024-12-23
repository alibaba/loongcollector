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
    EventsContainer& events = eGroup.MutableEvents();
    EventsContainer newEvents;
    newEvents.reserve(events.size());

    StringView scrapeTimestampMilliSecStr = eGroup.GetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_TIMESTAMP_MILLISEC);
    auto timestampMilliSec = StringTo<uint64_t>(scrapeTimestampMilliSecStr.to_string());
    auto timestamp = timestampMilliSec / 1000;
    auto nanoSec = timestampMilliSec % 1000 * 1000000;
    prom::TextParser parser(mScrapeConfigPtr->mHonorTimestamps);
    parser.SetDefaultTimestamp(timestamp, nanoSec);

    for (auto& e : events) {
        ProcessEvent(e, newEvents, eGroup, parser);
    }
    events.swap(newEvents);
    eGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_SAMPLES_SCRAPED, ToString(events.size()));
}

bool ProcessorPromParseMetricNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<RawEvent>();
}

bool ProcessorPromParseMetricNative::ProcessEvent(PipelineEventPtr& e,
                                                  EventsContainer& newEvents,
                                                  PipelineEventGroup& eGroup,
                                                  prom::TextParser& parser) {
    if (!IsSupportedEvent(e)) {
        return false;
    }
    auto& sourceEvent = e.Cast<RawEvent>();
    std::unique_ptr<MetricEvent> metricEvent = eGroup.CreateMetricEvent(true);
    if (parser.ParseLine(sourceEvent.GetContent(), *metricEvent)) {
        metricEvent->SetTag(string(prometheus::NAME), metricEvent->GetName());
        newEvents.emplace_back(std::move(metricEvent), true, nullptr);
    }
    return true;
}

} // namespace logtail
