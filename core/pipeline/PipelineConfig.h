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
#include "models/PipelineEventGroup.h"
#include "config/Config.h"

namespace logtail {

using PipelineConfig = Config; // should use json like object

class ComponentConfig {
public:
    ComponentConfig(const std::string& id, const PipelineConfig& config) : mId(id), mConfig(config) {}
    const std::string& GetId() const { return mId; }
    const Config& GetConfig() const { return mConfig; }

private:
    const std::string& mId;
    const Config& mConfig; // use Config temporarily
};

} // namespace logtail