/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "plugin/creator/CProcessor.h"
#include "plugin/creator/PluginCreator.h"

namespace logtail {

class DynamicCProcessorCreator : public PluginCreator {
public:
    DynamicCProcessorCreator(const processor_interface_t* plugin, void* handle);
    ~DynamicCProcessorCreator();
    const char* Name() override { return mPlugin ? mPlugin->name : ""; }
    bool IsDynamic() override { return true; }
    std::unique_ptr<PluginInstance> Create(const PluginInstance::PluginMeta& pluginMeta) override;

private:
    const processor_interface_t* mPlugin;
    void* _handle;
};

} // namespace logtail
