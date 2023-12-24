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
#include <vector>

#include <json/json.h>

#include "log_pb/sls_logs.pb.h"

namespace logtail {

struct DockerContainerPath {
    std::string mContainerID; // id of this container
    // container path for this config's path. eg, config path '/home/admin', container path
    // '/host_all/var/lib/xxxxxx/upper/home/admin' if config is wildcard, this will mapping to config->mWildcardPaths[0]
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

} // namespace logtail
