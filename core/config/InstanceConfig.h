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
#include <re2/re2.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace logtail {

struct InstanceConfig {
    std::string mName;
    std::unique_ptr<Json::Value> mDetail;

    // for alarm only
    std::string mProject;
    std::string mLogstore;
    std::string mRegion;

    InstanceConfig(const std::string& name, std::unique_ptr<Json::Value>&& detail)
        : mName(name), mDetail(std::move(detail)) {
        mProject = "";
        mLogstore = "";
        mRegion = "";
    }
    InstanceConfig(const logtail::InstanceConfig& config) {
        mName = config.mName;
        mDetail = std::make_unique<Json::Value>(*config.mDetail);
        mProject = "";
        mLogstore = "";
        mRegion = "";
    }

    InstanceConfig& operator=(InstanceConfig&& other) {
        if (this != &other) {
            mName = std::move(other.mName);
            mDetail = std::move(other.mDetail);
            mProject = "";
            mLogstore = "";
            mRegion = "";
        }
        return *this;
    }

    InstanceConfig& operator=(const InstanceConfig& other) {
        if (this != &other) {
            mName = other.mName;
            mDetail = std::make_unique<Json::Value>(*other.mDetail);
            mProject = "";
            mLogstore = "";
            mRegion = "";
        }
        return *this;
    }

    bool Parse() { return true; }

    const Json::Value& GetConfig() const { return *mDetail; }
};

inline bool operator==(const InstanceConfig& lhs, const InstanceConfig& rhs) {
    return (lhs.mName == rhs.mName) && (*lhs.mDetail == *rhs.mDetail);
}

inline bool operator!=(const InstanceConfig& lhs, const InstanceConfig& rhs) {
    return !(lhs == rhs);
}

} // namespace logtail
