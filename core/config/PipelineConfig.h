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

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace logtail {

struct PipelineConfig {
    std::string mName;
    std::unique_ptr<Json::Value> mDetail;
    uint32_t mCreateTime = 0;
    const Json::Value* mGlobal = nullptr;
    std::vector<const Json::Value*> mInputs;
    std::vector<const Json::Value*> mProcessors;
    std::vector<const Json::Value*> mAggregators;
    std::vector<const Json::Value*> mFlushers;
    std::vector<const Json::Value*> mExtensions;
    std::vector<std::pair<size_t, const Json::Value*>> mRouter;
    bool mHasNativeInput = false;
    bool mHasGoInput = false;
    bool mHasNativeProcessor = false;
    bool mHasGoProcessor = false;
    bool mHasNativeFlusher = false;
    bool mHasGoFlusher = false;
    bool mIsFirstProcessorJson = false;
    // for alarm only
    std::string mProject;
    std::string mLogstore;
    std::string mRegion;

    PipelineConfig(const std::string& name, std::unique_ptr<Json::Value>&& detail) : mName(name), mDetail(std::move(detail)) {}

    bool Parse();

    bool ShouldNativeFlusherConnectedByGoPipeline() const {
        // 过渡使用，待c++支持分叉后恢复下面的正式版
        return mHasGoProcessor || (mHasGoInput && !mHasNativeInput && mProcessors.empty())
            || (mHasGoFlusher && mHasNativeFlusher);
        // return mHasGoProcessor || (mHasGoInput && !mHasNativeInput && mProcessors.empty());
    }

    bool IsFlushingThroughGoPipelineExisted() const {
        return mHasGoFlusher || ShouldNativeFlusherConnectedByGoPipeline();
    }

    // bool IsProcessRunnerInvolved() const {
    //     // 长期过渡使用，待C++部分的时序聚合能力与Go持平后恢复下面的正式版
    //     return !(mHasGoInput && !mHasNativeProcessor);
    //     // return !(mHasGoInput && !mHasNativeProcessor && (mHasGoProcessor || (mHasGoFlusher && !mHasNativeFlusher)));
    // }

    bool HasGoPlugin() const { return mHasGoFlusher || mHasGoProcessor || mHasGoInput; }

    bool ReplaceEnvVar();
};

inline bool operator==(const PipelineConfig& lhs, const PipelineConfig& rhs) {
    return (lhs.mName == rhs.mName) && (*lhs.mDetail == *rhs.mDetail);
}

inline bool operator!=(const PipelineConfig& lhs, const PipelineConfig& rhs) {
    return !(lhs == rhs);
}

} // namespace logtail
