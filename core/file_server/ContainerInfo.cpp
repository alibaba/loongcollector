// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "file_server/ContainerInfo.h"

#include <memory>

#include "common/StringTools.h"
#include "logger/Logger.h"

namespace logtail {

const std::unordered_set<std::string> containerNameTag = {
    "_image_name_",
    "_container_name_",
    "_pod_name_",
    "_namespace_",
    "_pod_uid_",
    "_container_ip_",
};

bool ContainerInfo::ParseAllByJSONObj(const Json::Value& paramsAll,
                                      std::unordered_map<std::string, ContainerInfo>& containerInfoMap,
                                      std::string& errorMsg) {
    containerInfoMap.clear();

    // The json library changes its behavior for {"AllCmd":null}:
    // - v0.16.x: isArray() returns true.
    // - v1.x: isArray() returns false.
    // Do null check for compatibility.
    if (paramsAll.isNull()) {
        return true;
    }
    if (!paramsAll.isArray()) {
        errorMsg = "param is not of type array, param: " + paramsAll.toStyledString();
        return false;
    }
    for (auto iter = paramsAll.begin(); iter != paramsAll.end(); ++iter) {
        Json::Value params = *iter;
        ContainerInfo nowDCP;
        if (!ParseByJSONObj(params, nowDCP, errorMsg)) {
            errorMsg = "some container info is invalid: " + errorMsg;
            return false;
        }
        containerInfoMap[nowDCP.mID] = nowDCP;
    }
    return true;
}

bool ContainerInfo::ParseByJSONObj(const Json::Value& params, ContainerInfo& containerInfo, std::string& errorMsg) {
    bool isOldCheckpoint = !params.isMember("MetaDatas");
    containerInfo.mJson = params;
    if (params.isMember("ID") && params["ID"].isString()) {
        if (params["ID"].empty()) {
            errorMsg = "container id is empty, param: " + params.asString();
            return false;
        }
        containerInfo.mID = params["ID"].asString();
    }

    if (params.isMember("Mounts") && params["Mounts"].isArray()) {
        const Json::Value& mounts = params["Mounts"];
        for (Json::ArrayIndex i = 0; i < mounts.size(); ++i) {
            if (mounts[i].isMember("Source") && mounts[i]["Source"].isString() && mounts[i].isMember("Destination")
                && mounts[i]["Destination"].isString()) {
                std::string dst = mounts[i]["Destination"].asString();
                std::string src = mounts[i]["Source"].asString();
                // go传cmd时也做了path.clean, 这边兜底再判断下
                RemoveFilePathTrailingSlash(dst);
                RemoveFilePathTrailingSlash(src);
                containerInfo.mMounts.emplace_back(src, dst);
            }
        }
    }
    if (params.isMember("UpperDir") && params["UpperDir"].isString()) {
        containerInfo.mUpperDir = params["UpperDir"].asString();
        // go传cmd时也做了path.clean, 这边兜底再判断下
        RemoveFilePathTrailingSlash(containerInfo.mUpperDir);
    }
    if (params.isMember("LogPath") && params["LogPath"].isString()) {
        containerInfo.mLogPath = params["LogPath"].asString();
    }
    if (params.isMember("MetaDatas") && params["MetaDatas"].isArray()) {
        const Json::Value& metaDatas = params["MetaDatas"];
        for (Json::ArrayIndex i = 1; i < metaDatas.size(); i += 2) {
            if (metaDatas[i].isString() && metaDatas[i - 1].isString()) {
                sls_logs::LogTag tag;
                tag.set_key(metaDatas[i - 1].asString());
                tag.set_value(metaDatas[i].asString());
                containerInfo.mMetadatas.emplace_back(tag);
            }
        }
    }
    if (params.isMember("Tags") && params["Tags"].isArray()) {
        const Json::Value& tags = params["Tags"];
        for (Json::ArrayIndex i = 1; i < tags.size(); i += 2) {
            if (tags[i].isString() && tags[i - 1].isString()) {
                sls_logs::LogTag tag;
                tag.set_key(tags[i - 1].asString());
                tag.set_value(tags[i].asString());
                // 不是老版本
                if (!isOldCheckpoint) {
                    containerInfo.mTags.emplace_back(tag);
                } else if (containerNameTag.find(tags[i - 1].asString()) != containerNameTag.end()) {
                    containerInfo.mMetadatas.emplace_back(tag);
                } else {
                    containerInfo.mTags.emplace_back(tag);
                }
            }
        }
    }
    if (params.isMember("Path") && params["Path"].isString()) {
        containerInfo.mRealBaseDir = params["Path"].asString();
        LOG_INFO(sLogger,
                 ("recover container base dir from checkpoint", containerInfo.mRealBaseDir)("container id",
                                                                                            containerInfo.mID));
    }
    return true;
}

} // namespace logtail
