#include "plugin/instance/InputInstance.h"

namespace logtail {
bool InputInstance::Init(const Json::Value& config, PipelineContext& context) {
    mPlugin->SetContext(context);
    mPlugin->SetMetricsRecordRef(Name(), Id());
    if (mPlugin->Init(config)) {
        return false;
    }
    return true;
}
} // namespace logtail
