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

#include "file_server/DockerContainerPath.h"

#include <memory>

#include "logger/Logger.h"

namespace logtail {

bool DockerContainerPath::ParseAllByJSONStr(
    const std::string& jsonStr, std::unordered_map<std::string, DockerContainerPath>& dockerContainerPathMap) {
    dockerContainerPathMap.clear();
    Json::Value paramsAll;
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    std::unique_ptr<Json::CharReader> jsonReader(builder.newCharReader());
    std::string jsonParseErrs;
    if (jsonStr.size() < 5UL
        || !jsonReader->parse(jsonStr.data(), jsonStr.data() + jsonStr.size(), &paramsAll, &jsonParseErrs)) {
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
    dockerContainerPath.mJsonStr = jsonStr;
    Json::Value params;
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    std::unique_ptr<Json::CharReader> jsonReader(builder.newCharReader());
    std::string jsonParseErrs;
    if (jsonStr.size() < 5UL
        || !jsonReader->parse(jsonStr.data(), jsonStr.data() + jsonStr.size(), &params, &jsonParseErrs)) {
        LOG_ERROR(sLogger, ("invalid docker container params", jsonStr));
        return false;
    }
    return ParseByJSONObj(params, dockerContainerPath);
}

} // namespace logtail
