// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "config/ConfigUtil.h"

#include "common/FileSystemUtil.h"
#include "common/JsonUtil.h"
#include "common/YamlUtil.h"
#include "logger/Logger.h"

using namespace std;

namespace logtail {

bool LoadConfigDetailFromFile(const filesystem::path& filepath, Json::Value& detail) {
    const string& ext = filepath.extension().string();
    const string& configName = filepath.stem().string();
    if (configName == "region_config") {
        return false;
    }
    if (ext != ".yaml" && ext != ".yml" && ext != ".json") {
        LOG_WARNING(sLogger, ("unsupported config file format", "skip current object")("filepath", filepath));
        return false;
    }
    string content;
    if (!ReadFile(filepath.string(), content)) {
        LOG_WARNING(sLogger, ("failed to open config file", "skip current object")("filepath", filepath));
        return false;
    }
    if (content.empty()) {
        LOG_WARNING(sLogger, ("empty config file", "skip current object")("filepath", filepath));
        return false;
    }
    string errorMsg;
    if (!ParseConfigDetail(content, ext, detail, errorMsg)) {
        LOG_WARNING(sLogger,
                    ("config file format error", "skip current object")("error msg", errorMsg)("filepath", filepath));
        return false;
    }
    return true;
}

bool ParseConfigDetail(const string& content, const string& extension, Json::Value& detail, string& errorMsg) {
    if (extension == ".json") {
        return ParseJsonTable(content, detail, errorMsg);
    } else if (extension == ".yaml" || extension == ".yml") {
        YAML::Node yamlRoot;
        if (!ParseYamlTable(content, yamlRoot, errorMsg)) {
            return false;
        }
        detail = ConvertYamlToJson(yamlRoot);
        return true;
    }
    return false;
}

bool IsConfigEnabled(const string& name, const Json::Value& detail) {
    const char* key = "enable";
    const Json::Value* itr = detail.find(key, key + strlen(key));
    if (itr != nullptr) {
        if (!itr->isBool()) {
            LOG_WARNING(sLogger,
                        ("problem encountered in config parsing",
                         "param enable is not of type bool")("action", "ignore the config")("config", name));
            return false;
        }
        return itr->asBool();
    }
    return true;
}

ConfigType GetConfigType(const Json::Value& detail) {
    return detail.isMember("task") ? ConfigType::Task : ConfigType::Pipeline;
}

} // namespace logtail
