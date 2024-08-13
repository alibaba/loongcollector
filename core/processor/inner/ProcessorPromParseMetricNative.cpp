#include "processor/inner/ProcessorPromParseMetricNative.h"

#include <json/json.h>

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
    if (eGroup.GetEvents().empty()) {
        return;
    }

    EventsContainer& events = eGroup.MutableEvents();
    EventsContainer newEvents;

    for (auto& e : events) {
        ProcessEvent(e, newEvents, eGroup);
    }
    events.swap(newEvents);
}

bool ProcessorPromParseMetricNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<LogEvent>();
}

bool ProcessorPromParseMetricNative::ProcessEvent(PipelineEventPtr& e,
                                              EventsContainer& newEvents,
                                              PipelineEventGroup& eGroup) {
    if (!IsSupportedEvent(e)) {
        return false;
    }
    auto& sourceEvent = e.Cast<LogEvent>();
    std::unique_ptr<MetricEvent> metricEvent = eGroup.CreateMetricEvent();
    if (mParser.ParseLine(
            sourceEvent.GetContent(prometheus::PROMETHEUS).to_string(), *metricEvent, sourceEvent.GetTimestamp())) {
        newEvents.emplace_back(std::move(metricEvent));
    }
    return true;
}

} // namespace logtail
