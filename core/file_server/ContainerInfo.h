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

#include <cstdint>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "json/json.h"

#include "constants/TagConstants.h"

namespace logtail {

struct Mount {
    std::string mSource;
    std::string mDestination;
    Mount(const std::string& source, const std::string& destination) : mSource(source), mDestination(destination) {}
    Mount() = default;
};

struct K8sInfo {
    std::string mNamespace;
    std::string mPod;
    std::string mContainerName;
    std::unordered_map<std::string, std::string> mLabels;
    bool mPausedContainer = false;
};

struct RawContainerInfo {
    // 容器ID
    std::string mID;
    // 容器名称
    std::string mName;
    // 标准输出
    std::string mLogPath;
    // rootfs
    std::string mUpperDir;
    // 挂载
    std::vector<Mount> mMounts;
    // 容器信息
    std::vector<std::pair<TagKey, std::string>> mMetadatas;
    std::vector<std::pair<std::string, std::string>>
        mCustomMetadatas; //  ContainerNameTag which is custom, e.g. env config tag
    // 原始k8s信息
    K8sInfo mK8sInfo;
    // 环境变量信息
    std::unordered_map<std::string, std::string> mEnv;
    // 容器标签信息
    std::unordered_map<std::string, std::string> mContainerLabels;

    bool mStopped = false;

    std::string mStatus;

    bool operator==(const RawContainerInfo& rhs) const {
        if (mID != rhs.mID) {
            return false;
        }
        if (mName != rhs.mName) {
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
        if (mK8sInfo.mNamespace != rhs.mK8sInfo.mNamespace || mK8sInfo.mPod != rhs.mK8sInfo.mPod
            || mK8sInfo.mContainerName != rhs.mK8sInfo.mContainerName) {
            return false;
        }
        if (mK8sInfo.mLabels.size() != rhs.mK8sInfo.mLabels.size()) {
            return false;
        }
        for (const auto& label : mK8sInfo.mLabels) {
            const auto& rhsLabel = rhs.mK8sInfo.mLabels.find(label.first);
            if (rhsLabel == rhs.mK8sInfo.mLabels.end() || rhsLabel->second != label.second) {
                return false;
            }
        }
        if (mEnv.size() != rhs.mEnv.size()) {
            return false;
        }
        for (const auto& env : mEnv) {
            const auto& rhsEnv = rhs.mEnv.find(env.first);
            if (rhsEnv == rhs.mEnv.end() || rhsEnv->second != env.second) {
                return false;
            }
        }
        if (mContainerLabels.size() != rhs.mContainerLabels.size()) {
            return false;
        }
        for (const auto& label : mContainerLabels) {
            const auto& rhsLabel = rhs.mContainerLabels.find(label.first);
            if (rhsLabel == rhs.mContainerLabels.end() || rhsLabel->second != label.second) {
                return false;
            }
        }
        if (mStatus != rhs.mStatus) {
            return false;
        }
        return true;
    }
    bool operator!=(const RawContainerInfo& rhs) const { return !(*this == rhs); }

    void AddMetadata(const std::string& key, const std::string& value);
};


struct ContainerInfo {
    // 原始容器信息
    std::shared_ptr<RawContainerInfo> mRawContainerInfo;

    // container path for this config's path. eg, config path '/home/admin', container path
    // '/host_all/var/lib/xxxxxx/upper/home/admin' if config is wildcard, this will mapping to config->mWildcardPaths[0]
    std::string mRealBaseDir;

    std::vector<std::pair<std::string, std::string>> mExtraTags; // ExternalEnvTag and ExternalK8sLabelTag.

private:
};

} // namespace logtail
