#include "plugin/processor/inner/ProcessorPromDropMetricNative.h"

#include <json/json.h>

#include "models/MetricEvent.h"
#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"
#include "prometheus/PrometheusInputRunner.h"

using namespace std;
namespace logtail {

const string ProcessorPromDropMetricNative::sName = "processor_prom_drop_metric_native";

// only for inner processor
bool ProcessorPromDropMetricNative::Init(const Json::Value&) {
    mGlobalConfig = prom::PrometheusServer::GetInstance()->GetGlobalConfig();
    return true;
}

void ProcessorPromDropMetricNative::Process(PipelineEventGroup& eGroup) {
    EventsContainer& events = eGroup.MutableEvents();
    size_t wIdx = 0;
    for (size_t rIdx = 0; rIdx < events.size(); ++rIdx) {
        if (ProcessEvent(events[rIdx])) {
            if (wIdx != rIdx) {
                events[wIdx] = std::move(events[rIdx]);
            }
            ++wIdx;
        }
    }
    events.resize(wIdx);
}

bool ProcessorPromDropMetricNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<MetricEvent>();
}

bool ProcessorPromDropMetricNative::ProcessEvent(const PipelineEventPtr& e) {
    if (!IsSupportedEvent(e)) {
        return false;
    }
    const auto& sourceEvent = e.Cast<MetricEvent>();
    return !mGlobalConfig->IsDropped(sourceEvent.GetName());
}

} // namespace logtail
