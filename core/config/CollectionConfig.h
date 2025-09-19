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

#include <cstdint>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "json/json.h"

#include "config/PipelineConfig.h"

namespace logtail {

struct CollectionConfig : public PipelineConfig {
    const Json::Value* mGlobal = nullptr;
    std::vector<const Json::Value*> mInputs;
    std::optional<std::string> mSingletonInput;
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

    CollectionConfig(const std::string& name,
                     std::unique_ptr<Json::Value>&& detail,
                     const std::filesystem::path& filepath)
        : PipelineConfig(name, std::move(detail), filepath) {}
    CollectionConfig(CollectionConfig&& rhs) = default;
    CollectionConfig& operator=(CollectionConfig&& rhs) noexcept = default;

    bool Parse() override;

    bool ShouldNativeFlusherConnectedByGoPipeline() const {
        // 过渡使用，待c++支持分叉后恢复下面的正式版
        return mHasGoProcessor || (mHasGoInput && !mHasNativeInput && mProcessors.empty())
            || (mHasGoFlusher && mHasNativeFlusher);
        // return mHasGoProcessor || (mHasGoInput && !mHasNativeInput && mProcessors.empty());
    }

    bool IsFlushingThroughGoPipelineExisted() const {
        return mHasGoFlusher || ShouldNativeFlusherConnectedByGoPipeline();
    }

    bool ShouldAddProcessorTagNative() const { return mHasNativeProcessor || (mHasNativeInput && !mHasGoProcessor); }

    // bool IsProcessRunnerInvolved() const {
    //     // 长期过渡使用，待C++部分的时序聚合能力与Go持平后恢复下面的正式版
    //     return !(mHasGoInput && !mHasNativeProcessor);
    //     // return !(mHasGoInput && !mHasNativeProcessor && (mHasGoProcessor || (mHasGoFlusher &&
    //     !mHasNativeFlusher)));
    // }

    bool HasGoPlugin() const { return mHasGoFlusher || mHasGoProcessor || mHasGoInput; }

    bool IsOnetime() const { return mOnetimeExpireTime.has_value(); }

    bool ReplaceEnvVar();
};

inline bool operator==(const CollectionConfig& lhs, const CollectionConfig& rhs) {
    return static_cast<const PipelineConfig&>(lhs) == static_cast<const PipelineConfig&>(rhs);
}

inline bool operator!=(const CollectionConfig& lhs, const CollectionConfig& rhs) {
    return !(lhs == rhs);
}

} // namespace logtail
