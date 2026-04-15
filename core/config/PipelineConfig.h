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
#include <ctime>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "json/json.h"

namespace logtail {

struct PipelineConfig {
    std::string mName;
    std::unique_ptr<Json::Value> mDetail;
    uint64_t mConfigHash = 0;
    uint64_t mInputsHash = 0;
    std::filesystem::path mFilePath;
    uint32_t mCreateTime = 0;
    // valid for onetime config
    uint32_t mExcutionTimeout = 0;
    std::optional<uint32_t> mOnetimeStartTime;
    std::optional<uint32_t> mOnetimeExpireTime;
    bool mIsRunningBeforeStart = false;
    bool mForceRerunWhenUpdate = false;

    PipelineConfig(const std::string& name,
                   std::unique_ptr<Json::Value>&& detail,
                   const std::filesystem::path& filepath);
    PipelineConfig(PipelineConfig&& rhs) = default;
    PipelineConfig& operator=(PipelineConfig&& rhs) noexcept = default;
    virtual ~PipelineConfig() = default;

    virtual bool Parse() = 0;

#ifdef APSARA_UNIT_TEST_MAIN
    // Overridable clock for deterministic unit tests.
    // Placed in the public section so that the free helper GetCurrentTime() in
    // PipelineConfig.cpp can access it even under MSVC, which (unlike GCC/Clang
    // with -fno-access-control) enforces protected-member access strictly.
    static std::function<time_t()> sCurrentTime;

    // RAII guard that temporarily replaces the clock with a fixed value.
    struct ScopedClockOverride {
        explicit ScopedClockOverride(time_t fixedNow) : mPrev(sCurrentTime) {
            sCurrentTime = [fixedNow]() -> time_t { return fixedNow; };
        }
        ~ScopedClockOverride() { sCurrentTime = mPrev; }

        ScopedClockOverride(const ScopedClockOverride&) = delete;
        ScopedClockOverride& operator=(const ScopedClockOverride&) = delete;

    private:
        std::function<time_t()> mPrev;
    };
#endif

protected:
    bool GetExpireTimeIfOneTime(const Json::Value& global);

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PipelineConfigUnittest;
    friend class OnetimeConfigUpdateUnittest;
#endif
};

inline bool operator==(const PipelineConfig& lhs, const PipelineConfig& rhs) {
    return (lhs.mName == rhs.mName) && (*lhs.mDetail == *rhs.mDetail);
}

inline bool operator!=(const PipelineConfig& lhs, const PipelineConfig& rhs) {
    return !(lhs == rhs);
}

} // namespace logtail
