/*
 * Copyright 2025 iLogtail Authors
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

#include "json/json.h"

namespace logtail {

struct PipelineConfig {
    std::string mName;
    std::unique_ptr<Json::Value> mDetail;
    uint64_t mConfigHash = 0;
    std::filesystem::path mFilePath;
    uint32_t mCreateTime = 0;
    // valid for onetime config
    std::optional<uint32_t> mOnetimeStartTime;
    std::optional<uint32_t> mOnetimeExpireTime;
    bool mIsRunningBeforeStart = false;

    PipelineConfig(const std::string& name,
                   std::unique_ptr<Json::Value>&& detail,
                   const std::filesystem::path& filepath);
    PipelineConfig(PipelineConfig&& rhs) = default;
    PipelineConfig& operator=(PipelineConfig&& rhs) noexcept = default;
    virtual ~PipelineConfig() = default;

    virtual bool Parse() = 0;

protected:
    bool GetExpireTimeIfOneTime(const Json::Value& global);

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PipelineConfigUnittest;
#endif
};

inline bool operator==(const PipelineConfig& lhs, const PipelineConfig& rhs) {
    return (lhs.mName == rhs.mName) && (*lhs.mDetail == *rhs.mDetail);
}

inline bool operator!=(const PipelineConfig& lhs, const PipelineConfig& rhs) {
    return !(lhs == rhs);
}

} // namespace logtail
