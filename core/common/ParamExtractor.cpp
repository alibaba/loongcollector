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

#include "common/ParamExtractor.h"

#include "boost/regex.hpp"

using namespace std;

namespace logtail {

string ExtractCurrentKey(const string& key) {
    size_t pos = key.rfind('.');
    if (pos == string::npos) {
        return key;
    }
    return key.substr(pos + 1);
}

bool GetOptionalBoolParam(const Json::Value& config, const string& key, bool& param, string& errorMsg) {
    errorMsg.clear();
    string curKey = ExtractCurrentKey(key);
    const Json::Value* itr = config.find(curKey.c_str(), curKey.c_str() + curKey.length());
    if (itr != nullptr) {
        if (!itr->isBool()) {
            errorMsg = "param " + key + " is not of type bool";
            return false;
        }
        param = itr->asBool();
    }
    return true;
}

bool GetOptionalIntParam(const Json::Value& config, const string& key, int32_t& param, string& errorMsg) {
    errorMsg.clear();
    string curKey = ExtractCurrentKey(key);
    const Json::Value* itr = config.find(curKey.c_str(), curKey.c_str() + curKey.length());
    if (itr != nullptr) {
        if (!itr->isInt()) {
            errorMsg = "param " + key + " is not of type int";
            return false;
        }
        param = itr->asInt();
    }
    return true;
}

bool GetOptionalUIntParam(const Json::Value& config, const string& key, uint32_t& param, string& errorMsg) {
    errorMsg.clear();
    string curKey = ExtractCurrentKey(key);
    const Json::Value* itr = config.find(curKey.c_str(), curKey.c_str() + curKey.length());
    if (itr != nullptr) {
        if (!itr->isUInt()) {
            errorMsg = "param " + key + " is not of type uint";
            return false;
        }
        param = itr->asUInt();
    }
    return true;
}

bool GetOptionalStringParam(const Json::Value& config, const string& key, string& param, string& errorMsg) {
    errorMsg.clear();
    string curKey = ExtractCurrentKey(key);
    const Json::Value* itr = config.find(curKey.c_str(), curKey.c_str() + curKey.length());
    if (itr != nullptr) {
        if (!itr->isString()) {
            errorMsg = "param " + key + " is not of type string";
            return false;
        }
        param = itr->asString();
    }
    return true;
}

bool GetOptionalDoubleParam(const Json::Value& config, const string& key, double& param, string& errorMsg) {
    errorMsg.clear();
    string curKey = ExtractCurrentKey(key);
    const Json::Value* itr = config.find(curKey.c_str(), curKey.c_str() + curKey.length());
    if (itr != nullptr) {
        if (!itr->isDouble()) {
            errorMsg = "param " + key + " is not of type double";
            return false;
        }
        param = itr->asDouble();
    }
    return true;
}

bool GetMandatoryBoolParam(const Json::Value& config, const string& key, bool& param, string& errorMsg) {
    errorMsg.clear();
    if (!config.isMember(ExtractCurrentKey(key))) {
        errorMsg = "madatory param " + key + " is missing";
        return false;
    }
    return GetOptionalBoolParam(config, key, param, errorMsg);
}

bool GetMandatoryIntParam(const Json::Value& config, const string& key, int32_t& param, string& errorMsg) {
    errorMsg.clear();
    if (!config.isMember(ExtractCurrentKey(key))) {
        errorMsg = "madatory param " + key + " is missing";
        return false;
    }
    return GetOptionalIntParam(config, key, param, errorMsg);
}

bool GetMandatoryUIntParam(const Json::Value& config, const string& key, uint32_t& param, string& errorMsg) {
    errorMsg.clear();
    if (!config.isMember(ExtractCurrentKey(key))) {
        errorMsg = "madatory param " + key + " is missing";
        return false;
    }
    return GetOptionalUIntParam(config, key, param, errorMsg);
}

bool GetMandatoryStringParam(const Json::Value& config, const string& key, string& param, string& errorMsg) {
    errorMsg.clear();
    if (!config.isMember(ExtractCurrentKey(key))) {
        errorMsg = "madatory param " + key + " is missing";
        return false;
    }
    if (!GetOptionalStringParam(config, key, param, errorMsg)) {
        return false;
    }
    if (param.empty()) {
        errorMsg = "madatory string param " + key + " is empty";
        return false;
    }
    return true;
}

bool GetMandatoryDoubleParam(const Json::Value& config, const std::string& key, double& param, std::string& errorMsg) {
    errorMsg.clear();
    if (!config.isMember(ExtractCurrentKey(key))) {
        errorMsg = "madatory param " + key + " is missing";
        return false;
    }
    return GetOptionalDoubleParam(config, key, param, errorMsg);
}

bool IsRegexValid(const string& regStr) {
    if (regStr.empty()) {
        return true;
    }
    try {
        boost::regex reg(regStr);
    } catch (...) {
        return false;
    }
    return true;
}

bool IsValidList(const Json::Value& config, const string& key, string& errorMsg) {
    errorMsg.clear();
    string curKey = ExtractCurrentKey(key);
    const Json::Value* itr = config.find(curKey.c_str(), curKey.c_str() + curKey.length());
    if (itr == nullptr) {
        errorMsg = "param " + key + " is missing";
        return false;
    }
    if (!itr->isArray()) {
        errorMsg = "param " + key + " is not of type list";
        return false;
    }
    return true;
}

bool IsValidMap(const Json::Value& config, const string& key, string& errorMsg) {
    errorMsg.clear();
    string curKey = ExtractCurrentKey(key);
    const Json::Value* itr = config.find(curKey.c_str(), curKey.c_str() + curKey.length());
    if (itr == nullptr) {
        errorMsg = "param " + key + " is missing";
        return false;
    }
    if (!itr->isObject()) {
        errorMsg = "param " + key + " is not of type map";
        return false;
    }
    return true;
}

static void ParseDefaultAddedTag(const Json::Value* config,
                                 const string& configField,
                                 const string& defaultTagKeyValue,
                                 const CollectionPipelineContext& context,
                                 const string& pluginType,
                                 string& customTagKey) {
    string errorMsg;
    customTagKey = DEFAULT_CONFIG_TAG_KEY_VALUE;
    if (config && config->isMember(configField)) {
        if (!GetOptionalStringParam(*config, "Tags." + configField, customTagKey, errorMsg)) {
            PARAM_WARNING_DEFAULT(context.GetLogger(),
                                  context.GetAlarm(),
                                  errorMsg,
                                  customTagKey,
                                  pluginType,
                                  context.GetConfigName(),
                                  context.GetProjectName(),
                                  context.GetLogstoreName(),
                                  context.GetRegion());
        }
        if (customTagKey == DEFAULT_CONFIG_TAG_KEY_VALUE) {
            customTagKey = defaultTagKeyValue;
        }
    } else {
        customTagKey = defaultTagKeyValue;
    }
}

static void ParseOptionalTag(const Json::Value* config,
                             const string& configField,
                             const string& defaultTagKeyValue,
                             const CollectionPipelineContext& context,
                             const string& pluginType,
                             string& customTagKey) {
    string errorMsg;
    if (config && config->isMember(configField)) {
        if (!GetOptionalStringParam(*config, "Tags." + configField, customTagKey, errorMsg)) {
            PARAM_WARNING_DEFAULT(context.GetLogger(),
                                  context.GetAlarm(),
                                  errorMsg,
                                  customTagKey,
                                  pluginType,
                                  context.GetConfigName(),
                                  context.GetProjectName(),
                                  context.GetLogstoreName(),
                                  context.GetRegion());
        }
        if (customTagKey == DEFAULT_CONFIG_TAG_KEY_VALUE) {
            customTagKey = defaultTagKeyValue;
        }
    } else {
        customTagKey = "";
    }
}

// if there is no tag config, config maybe nullptr, will act as default (default added or optional)
void ParseTagKey(const Json::Value* config,
                 const string& configField,
                 TagKey tagKey,
                 unordered_map<TagKey, string>& tagKeyMap,
                 const CollectionPipelineContext& context,
                 const std::string& pluginType,
                 bool defaultAdded) {
    string customTagKey;
    if (defaultAdded) {
        ParseDefaultAddedTag(config, configField, GetDefaultTagKeyString(tagKey), context, pluginType, customTagKey);
    } else {
        ParseOptionalTag(config, configField, GetDefaultTagKeyString(tagKey), context, pluginType, customTagKey);
    }
    if (!customTagKey.empty()) {
        tagKeyMap[tagKey] = customTagKey;
    }
}

} // namespace logtail
