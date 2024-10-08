#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "pipeline/plugin/interface/Input.h"
#include "prometheus/schedulers/TargetSubscriberScheduler.h"

namespace logtail {

class InputPrometheus : public Input {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override;
    bool Start() override;
    bool Stop(bool isPipelineRemoving) override;
    bool SupportAck() const override { return true; }

private:
    bool CreateInnerProcessors(const Json::Value& inputConfig);
    // only one job is supported
    std::shared_ptr<TargetSubscriberScheduler> mTargetSubscirber;

    std::string mJobName;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class InputPrometheusUnittest;
#endif
};

} // namespace logtail