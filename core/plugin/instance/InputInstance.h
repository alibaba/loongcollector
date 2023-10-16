#pragma once

#include <memory>

#include "plugin/instance/PluginInstance.h"
#include "plugin/interface/Input.h"
// #include "table/Table.h"

namespace logtail {
class InputInstance : public PluginInstance {
public:
    InputInstance(Input* plugin, const std::string& pluginId) : PluginInstance(pluginId), mPlugin(plugin) {}

    const std::string& Name() const override { return mPlugin->Name(); }
    // bool Init(const Table& config, PipelineContext& context);
    bool Init(const Json::Value& config, PipelineContext& context);
    void Start() { mPlugin->Start(); }
    void Stop(bool isPipelineRemoving) { mPlugin->Stop(isPipelineRemoving); }

private:
    std::unique_ptr<Input> mPlugin;
};
} // namespace logtail
