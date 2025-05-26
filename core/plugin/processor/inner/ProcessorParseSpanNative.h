#pragma once

#include <string>

#include "collection_pipeline/plugin/interface/Processor.h"
#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"

namespace logtail {
class ProcessorParseSpanNative : public Processor {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config) override;
    void Process(PipelineEventGroup&) override;

protected:
    bool IsSupportedEvent(const PipelineEventPtr&) const override;

private:
    bool ProcessEvent(PipelineEventPtr&, EventsContainer&, PipelineEventGroup&);
    // TODO listen to config for multi protocol version

#ifdef APSARA_UNIT_TEST_MAIN
    // TODO unittest
#endif
};

} // namespace logtail
