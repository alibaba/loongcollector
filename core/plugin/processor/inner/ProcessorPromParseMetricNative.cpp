#include "plugin/processor/inner/ProcessorPromParseMetricNative.h"

#include <json/json.h>

#include "common/StringTools.h"
#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"
#include "prometheus/Constants.h"

using namespace std;
namespace logtail {

const string ProcessorPromParseMetricNative::sName = "processor_prom_parse_metric_native";

// only for inner processor
bool ProcessorPromParseMetricNative::Init(const Json::Value&) {
    return true;
}

void ProcessorPromParseMetricNative::Process(PipelineEventGroup& eGroup) {
    EventsContainer& events = eGroup.MutableEvents();
    EventsContainer newEvents;

    StringView scrapeTimestampMilliSecStr = eGroup.GetMetadata(EventGroupMetaKey::PROMETHEUS_SCRAPE_TIMESTAMP_MILLISEC);
    auto timestampMilliSec = StringTo<uint64_t>(scrapeTimestampMilliSecStr.to_string());
    auto timestamp = timestampMilliSec / 1000;
    auto nanoSec = timestampMilliSec % 1000 * 1000000;

    for (auto& e : events) {
        ProcessEvent(e, newEvents, eGroup, timestamp, nanoSec);
    }
    events.swap(newEvents);
    eGroup.SetMetadata(EventGroupMetaKey::PROMETHEUS_SAMPLES_SCRAPED, ToString(events.size()));
}

bool ProcessorPromParseMetricNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<LogEvent>();
}

bool ProcessorPromParseMetricNative::ProcessEvent(
    PipelineEventPtr& e, EventsContainer& newEvents, PipelineEventGroup& eGroup, uint64_t timestamp, uint32_t nanoSec) {
    if (!IsSupportedEvent(e)) {
        return false;
    }
    auto& sourceEvent = e.Cast<LogEvent>();
    std::unique_ptr<MetricEvent> metricEvent = eGroup.CreateMetricEvent();
    if (mParser.ParseLine(sourceEvent.GetContent(prometheus::PROMETHEUS), timestamp, nanoSec, *metricEvent)) {
        newEvents.emplace_back(std::move(metricEvent));
    }
    return true;
}

} // namespace logtail
