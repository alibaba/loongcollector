#pragma once

#include <string>

#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"
#include "pipeline/plugin/interface/Processor.h"
#include "prometheus/labels/TextParser.h"
#include "prometheus/schedulers/ScrapeConfig.h"

namespace logtail {
class ProcessorPromParseMetricNative : public Processor {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config) override;
    void Process(PipelineEventGroup&) override;

protected:
    bool IsSupportedEvent(const PipelineEventPtr&) const override;

private:
    bool ProcessEvent(PipelineEventPtr&, EventsContainer&, PipelineEventGroup&, TextParser& parser);
    std::unique_ptr<ScrapeConfig> mScrapeConfigPtr;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class InputPrometheusUnittest;
#endif
};

} // namespace logtail
