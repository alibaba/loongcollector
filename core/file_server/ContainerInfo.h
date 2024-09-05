/*
 * Copyright 2022 iLogtail Authors
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

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "container_manager/ConfigContainerInfoUpdateCmd.h"
#include "protobuf/sls/sls_logs.pb.h"

namespace logtail {

struct Mount {
    std::string mSource;
    std::string mDestination;
    Mount(const std::string& source, const std::string& destination) : mSource(source), mDestination(destination) {}
    Mount() = default;
};

struct ContainerInfo {
    std::string mID; // id of this container
    // container path for this config's path. eg, config path '/home/admin', container path
    // '/host_all/var/lib/xxxxxx/upper/home/admin' if config is wildcard, this will mapping to config->mWildcardPaths[0]
    std::string mRealBaseDir;

    std::string mLogPath;
    std::string mUpperDir;
    std::vector<Mount> mMounts; // mounts of this container
    std::vector<sls_logs::LogTag> mTags; // ContainerNameTag
    std::vector<sls_logs::LogTag> mMetadatas; // ExternalEnvTag and ExternalK8sLabelTag
    Json::Value mJson; // this obj's json, for saving to local file

    static bool ParseByJSONObj(const Json::Value&, ContainerInfo&, std::string&);
    static bool ParseAllByJSONObj(const Json::Value&, std::unordered_map<std::string, ContainerInfo>&, std::string&);

    bool operator==(const ContainerInfo& rhs) const {
        if (mID != rhs.mID) {
            return false;
        }
        if (mRealBaseDir != rhs.mRealBaseDir) {
            return false;
        }
        if (mLogPath != rhs.mLogPath) {
            return false;
        }
        if (mUpperDir != rhs.mUpperDir) {
            return false;
        }
        if (mMounts.size() != rhs.mMounts.size()) {
            return false;
        }
        for (size_t idx = 0; idx < mMounts.size(); ++idx) {
            const auto& lhsMount = mMounts[idx];
            const auto& rhsMount = rhs.mMounts[idx];
            if (lhsMount.mSource != rhsMount.mSource || lhsMount.mDestination != rhsMount.mDestination) {
                return false;
            }
        }
        if (mMetadatas.size() != rhs.mMetadatas.size()) {
            return false;
        }
        for (size_t idx = 0; idx < mMetadatas.size(); ++idx) {
            const auto& lhsTag = mMetadatas[idx];
            const auto& rhsTag = rhs.mMetadatas[idx];
            if (lhsTag.key() != rhsTag.key() || lhsTag.value() != rhsTag.value()) {
                return false;
            }
        }
        if (mTags.size() != rhs.mTags.size()) {
            return false;
        }
        for (size_t idx = 0; idx < mTags.size(); ++idx) {
            const auto& lhsTag = mTags[idx];
            const auto& rhsTag = rhs.mTags[idx];
            if (lhsTag.key() != rhsTag.key() || lhsTag.value() != rhsTag.value()) {
                return false;
            }
        }
        return true;
    }
    bool operator!=(const ContainerInfo& rhs) const { return !(*this == rhs); }

private:
};

} // namespace logtail
