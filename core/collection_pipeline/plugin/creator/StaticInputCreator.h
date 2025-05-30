/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "collection_pipeline/plugin/creator/PluginCreator.h"
#include "collection_pipeline/plugin/instance/InputInstance.h"

namespace logtail {

template <typename T>
class StaticInputCreator : public PluginCreator {
public:
    const char* Name() override { return T::sName.c_str(); }
    bool IsDynamic() override { return false; }
    std::unique_ptr<PluginInstance> Create(const PluginInstance::PluginMeta& pluginMeta) override {
        return std::make_unique<InputInstance>(new T, pluginMeta);
    }
};

} // namespace logtail
