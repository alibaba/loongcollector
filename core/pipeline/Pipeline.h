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

#include <json/json.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "config/Config.h"
#include "models/PipelineEventGroup.h"
#include "pipeline/PipelineContext.h"
#include "plugin/instance/FlusherInstance.h"
#include "plugin/instance/InputInstance.h"
#include "plugin/instance/ProcessorInstance.h"

namespace logtail {

class Pipeline {
public:
    // copy/move control functions are deleted because of mContext
    bool Init(Config&& config);
    void Start();
    void Process(std::vector<PipelineEventGroup>& logGroupList);
    void Stop(bool isRemoving);

    const std::string& Name() const { return mName; }
    PipelineContext& GetContext() const { return mContext; }
    const Json::Value& GetConfig() const { return *mConfig; }
    const std::vector<std::unique_ptr<FlusherInstance>>& GetFlushers() const { return mFlushers; }
    bool IsFlushingThroughGoPipeline() const { return !mGoPipelineWithoutInput.isNull(); }
    const std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>>& GetPluginStatistics() const {
        return mPluginCntMap;
    }
    bool LoadGoPipelines() const; // 应当放在private，过渡期间放在public

    // only for input_observer_network for compatability
    const std::vector<std::unique_ptr<InputInstance>>& GetInputs() const { return mInputs; }

private:
    void MergeGoPipeline(const Json::Value& src, Json::Value& dst);
    void AddPluginToGoPipeline(const Json::Value& plugin, const std::string& module, Json::Value& dst);
    void CopyNativeGlobalParamToGoPipeline(Json::Value& root);
    bool ShouldAddPluginToGoPipelineWithInput() const { return mInputs.empty() && mProcessorLine.empty(); }

    std::string mName;
    std::vector<std::unique_ptr<InputInstance>> mInputs;
    std::vector<std::unique_ptr<ProcessorInstance>> mProcessorLine;
    std::vector<std::unique_ptr<FlusherInstance>> mFlushers;
    Json::Value mGoPipelineWithInput;
    Json::Value mGoPipelineWithoutInput;
    mutable PipelineContext mContext;
    std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> mPluginCntMap;
    std::unique_ptr<Json::Value> mConfig;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PipelineMock;
    friend class PipelineUnittest;
    friend class InputFileUnittest;
    friend class ProcessorTagNativeUnittest;
#endif
};

} // namespace logtail
