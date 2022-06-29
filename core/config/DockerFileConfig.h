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
#include <string>
#include <json/json.h>
#include "log_pb/sls_logs.pb.h"

namespace logtail {

/**
 * @brief The DockerContainerPath struct, for docker file and container mode
 */
struct DockerContainerPath {
    std::string mContainerID; // id of this container
    // container path for this config's path. eg, config path '/home/admin', container path '/host_all/var/lib/xxxxxx/upper/home/admin'
    // if config is wildcard, this will mapping to config->mWildcardPaths[0]
    std::string mContainerPath;
    std::vector<sls_logs::LogTag> mContainerTags; // tags extracted from this container
    std::string mJsonStr; // this obj's json string, for saving to local file

    static bool ParseByJSONStr(const std::string& jsonStr, DockerContainerPath& dockerContainerPath);
    static bool ParseAllByJSONStr(const std::string& jsonStr,
                                  std::unordered_map<std::string, DockerContainerPath>& dockerContainerPathMap);

    bool operator==(const DockerContainerPath& rhs) const {
        if (mContainerID != rhs.mContainerID) {
            return false;
        }
        if (mContainerPath != rhs.mContainerPath) {
            return false;
        }
        if (mContainerTags.size() != rhs.mContainerTags.size()) {
            return false;
        }
        for (size_t idx = 0; idx < mContainerTags.size(); ++idx) {
            const auto& lhsTag = mContainerTags[idx];
            const auto& rhsTag = rhs.mContainerTags[idx];
            if (lhsTag.key() != rhsTag.key() || lhsTag.value() != rhsTag.value()) {
                return false;
            }
        }
        return true;
    }
    bool operator!=(const DockerContainerPath& rhs) const { return !(*this == rhs); }

private:
    static bool ParseByJSONObj(const Json::Value& jsonObj, DockerContainerPath& dockerContainerPath);
};

/**
 * @brief The DockerContainerPathCmd struct. for docker file only
 */
struct DockerContainerPathCmd {
    std::string mConfigName; // config name
    bool mDeleteFlag; // if this flag is true, delete the container from this config's DockerContainerPath array
    bool
        mUpdateAllFlag; // if this flag is true, clear this config's DockerContainerPath array and load all DockerContainerPaths
    std::string
        mParams; // params, json string. this has 2 types : 1 one DockerContainerPath, 2 multi DockerContainerPaths, just when mUpdateAllFlag is true

    DockerContainerPathCmd(const std::string& configName, bool delFlag, const std::string& params, bool allFlag)
        : mConfigName(configName), mDeleteFlag(delFlag), mUpdateAllFlag(allFlag), mParams(params) {}
};

/**
 * @brief The DockerMountPaths struct. for container mode, load from local config file
 */
struct DockerMountPaths {
    /**
     * @brief The MountPath struct. a mount path
     */
    struct MountPath {
        std::string source; // eg, `/home/admin/logs`
        std::string destination; // eg, `/home/admin/t4/docker/overlay/c89d95d99442336a1afab95c320905c/upper`
    };
    DockerMountPaths() {}
    std::string mVersion; // version of this config file
    std::string mContainerName; // container name
    std::string mContainerID; // container ID
    std::string mHostPath; // host volume path
    std::vector<MountPath> mMountPathArray; // all MountPath of this container
    std::string mJsonStr; // this obj's json string, for checking difference

    /**
     * @brief find best mount path of specific source. eg source `/home/admin/logs/access`, mountRealPath is `/host_all/home/admin/t4/docker/overlay/xxxxx/upper/access`
     * @param source, config's path
     * @param mountRealPath, the real path which logtail can access
     * @return
     */
    bool FindBestMountPath(const std::string source, std::string& mountRealPath);
    static bool ParseByJsonStr(const std::string& paramJSONStr, DockerMountPaths& dockerMountPaths);
};

} // namespace logtail
