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

#include "DockerFileConfig.h"
#include "logger/Logger.h"
#include "common/FileSystemUtil.h"

namespace logtail {

bool DockerContainerPath::ParseAllByJSONStr(
    const std::string& jsonStr, std::unordered_map<std::string, DockerContainerPath>& dockerContainerPathMap) {
    dockerContainerPathMap.clear();
    Json::Value paramsAll;
    Json::Reader reader;
    if (jsonStr.size() < (size_t)5 || !reader.parse(jsonStr, paramsAll)) {
        LOG_ERROR(sLogger, ("invalid docker container params error", jsonStr));
        return false;
    }
    if (!paramsAll.isMember("AllCmd")) {
        return false;
    }

    auto paramsArray = paramsAll["AllCmd"];
    // The json library changes its behavior for {"AllCmd":null}:
    // - v0.16.x: isArray() returns true.
    // - v1.x: isArray() returns false.
    // Do null check for compatibility.
    if (paramsArray.isNull()) {
        return true;
    }
    if (!paramsArray.isArray()) {
        return false;
    }
    for (Json::Value::iterator iter = paramsArray.begin(); iter != paramsArray.end(); ++iter) {
        Json::Value params = *iter;
        DockerContainerPath nowDCP;
        nowDCP.mJsonStr = params.toStyledString();
        if (!ParseByJSONObj(params, nowDCP)) {
            LOG_ERROR(sLogger, ("parse sub docker container params error", nowDCP.mJsonStr));
            return false;
        }
        dockerContainerPathMap[nowDCP.mContainerID] = nowDCP;
    }
    return true;
}

bool DockerContainerPath::ParseByJSONObj(const Json::Value& params, DockerContainerPath& dockerContainerPath) {
    if (params.isMember("ID") && params["ID"].isString()) {
        dockerContainerPath.mContainerID = params["ID"].asString();
    }
    if (params.isMember("Path") && params["Path"].isString()) {
        dockerContainerPath.mContainerPath = params["Path"].asString();
    }
    if (params.isMember("Tags") && params["Tags"].isArray()) {
        const Json::Value& tags = params["Tags"];
        for (Json::ArrayIndex i = 1; i < tags.size(); i += 2) {
            if (tags[i].isString() && tags[i - 1].isString()) {
                sls_logs::LogTag tag;
                tag.set_key(tags[i - 1].asString());
                tag.set_value(tags[i].asString());
                dockerContainerPath.mContainerTags.push_back(tag);
            }
        }
    }
    // only check containerID (others are null when parse delete cmd)
    if (dockerContainerPath.mContainerID.size() > 0) {
        return true;
    }
    return false;
}

bool DockerContainerPath::ParseByJSONStr(const std::string& jsonStr, DockerContainerPath& dockerContainerPath) {
    Json::Value params;
    Json::Reader reader;
    dockerContainerPath.mJsonStr = jsonStr;
    if (jsonStr.size() < (size_t)5 || !reader.parse(jsonStr, params)) {
        LOG_ERROR(sLogger, ("invalid docker container params", jsonStr));
        return false;
    }
    return ParseByJSONObj(params, dockerContainerPath);
}

bool DockerMountPaths::FindBestMountPath(const std::string source, std::string& mountRealPath) {
    size_t maxSameLen = 0;
    size_t maxSameIndex = 0;

    size_t sourceSize = source.size();
    if (sourceSize < (size_t)1 || source[0] != '/') {
        return false;
    }
    for (size_t i = 0; i < mMountPathArray.size(); ++i) {
        const MountPath& mountPath = mMountPathArray[i];
        size_t mountDestSize = mountPath.destination.size();
        // must check '/' first
        if (mountDestSize == (size_t)1) {
            if (mountDestSize > maxSameLen) {
                maxSameLen = mountDestSize;
                maxSameIndex = i;
            }
        } else if (mountDestSize <= source.size()) {
            if (memcmp(source.c_str(), mountPath.destination.c_str(), mountDestSize) == 0
                && (mountDestSize == sourceSize || source[mountDestSize] == '/')) {
                if (mountDestSize > maxSameLen) {
                    maxSameLen = mountDestSize;
                    maxSameIndex = i;
                }
            }
        }
    }

    if (maxSameLen == 0) {
        LOG_ERROR(sLogger,
                  ("can not find best mount path, docker mount path is error, content", mJsonStr)("source", source));
        return false;
    }
    std::string leftPath = source.substr(maxSameLen);
    // source '/', mountPath.destination '/'
    if (leftPath.empty()) {
        mountRealPath = mHostPath + mMountPathArray[maxSameIndex].source;
    }
    // source '/a/b/c', mountPath.destination '/a'
    else if (leftPath[0] == '/') {
        mountRealPath = mHostPath + mMountPathArray[maxSameIndex].source + leftPath;
    }
    // source '/a/b/c', mountPath.destination '/'
    else if (mHostPath.size() == (size_t)1 && mHostPath[0] == '/') {
        mountRealPath = mMountPathArray[maxSameIndex].source + '/' + leftPath;
    } else {
        mountRealPath = mHostPath + mMountPathArray[maxSameIndex].source + '/' + leftPath;
    }

    TrimLastSeperator(mountRealPath);
    LOG_INFO(sLogger, ("FindBestMountPath source", source)("real path", mountRealPath));
    return true;
}

bool DockerMountPaths::ParseByJsonStr(const std::string& paramJSONStr, DockerMountPaths& dockerMountPaths) {
    Json::Value paramJSON;
    Json::Reader reader;
    dockerMountPaths.mMountPathArray.clear();
    dockerMountPaths.mJsonStr = paramJSONStr;
    if (paramJSONStr.size() < (size_t)5 || !reader.parse(paramJSONStr, paramJSON)) {
        LOG_ERROR(sLogger, ("invalid docker mount path param", paramJSONStr));
        return false;
    }

    if (paramJSON.isMember("version") && paramJSON["version"].isString()) {
        dockerMountPaths.mVersion = paramJSON["version"].asString();
    }

    if (paramJSON.isMember("container_name") && paramJSON["container_name"].isString()) {
        dockerMountPaths.mContainerName = paramJSON["container_name"].asString();
    }

    if (paramJSON.isMember("container_id") && paramJSON["container_id"].isString()) {
        dockerMountPaths.mContainerID = paramJSON["container_id"].asString();
    }

    if (paramJSON.isMember("host_path") && paramJSON["host_path"].isString()) {
        dockerMountPaths.mHostPath = paramJSON["host_path"].asString();
        TrimLastSeperator(dockerMountPaths.mHostPath);
    }

    if (paramJSON.isMember("container_mount") && paramJSON["container_mount"].isArray()) {
        const Json::Value& mountObjs = paramJSON["container_mount"];
        for (Json::ArrayIndex i = 0; i < mountObjs.size(); ++i) {
            bool parseResult = false;
            const Json::Value& mountObj = mountObjs[i];
            if (mountObj.isObject()) {
                MountPath mountPath;
                bool sourceFlag = false;
                bool destinationFlag = false;
                if (mountObj.isMember("source") && mountObj["source"].isString()) {
                    sourceFlag = true;
                    mountPath.source = mountObj["source"].asString();
                    TrimLastSeperator(mountPath.source);
                }

                if (mountObj.isMember("destination") && mountObj["destination"].isString()) {
                    destinationFlag = true;
                    mountPath.destination = mountObj["destination"].asString();
                    TrimLastSeperator(mountPath.destination);
                }

                if (destinationFlag && sourceFlag) {
                    parseResult = true;
                    dockerMountPaths.mMountPathArray.push_back(mountPath);
                }
            }
            if (!parseResult) {
                LOG_ERROR(sLogger, ("parse mount path error, content", mountObj.toStyledString()));
            }
        }
    }
    if (dockerMountPaths.mVersion.empty() || dockerMountPaths.mContainerID.empty()
        || dockerMountPaths.mContainerName.empty() || dockerMountPaths.mHostPath.empty()
        || dockerMountPaths.mMountPathArray.empty()) {
        LOG_ERROR(sLogger,
                  ("invalid docker mount paths, missing some params, content",
                   dockerMountPaths.mJsonStr)("\n", "params")(dockerMountPaths.mVersion, dockerMountPaths.mContainerID)(
                      dockerMountPaths.mContainerName,
                      dockerMountPaths.mHostPath)("mount array size", dockerMountPaths.mMountPathArray.size()));
        return false;
    }
    LOG_INFO(sLogger, ("load docker mount paths success", "content\n")(paramJSONStr, ""));
    return true;
}

} // namespace logtail
