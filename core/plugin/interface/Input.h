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

#include <json/json.h>

#include <memory>
#include <vector>

#include "plugin/instance/ProcessorInstance.h"
#include "plugin/interface/Plugin.h"

namespace logtail {

class Input : public Plugin {
public:
    virtual ~Input() = default;

    virtual bool Init(const Json::Value& config, uint32_t& pluginIdx, Json::Value& optionalGoPipeline) = 0;
    virtual bool Start() = 0;
    virtual bool Stop(bool isPipelineRemoving) = 0;

    void SetInputIndex(size_t idx) { mIndex = idx; }
    std::vector<std::unique_ptr<ProcessorInstance>>& GetInnerProcessors() { return mInnerProcessors; }

protected:
    size_t mIndex = 0;
    std::vector<std::unique_ptr<ProcessorInstance>> mInnerProcessors;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class InputInstanceUnittest;
#endif
};

} // namespace logtail
