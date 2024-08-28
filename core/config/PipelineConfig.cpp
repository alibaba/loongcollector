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

#include "config/PipelineConfig.h"

#include <string>

#include "app_config/AppConfig.h"
#include "common/FileSystemUtil.h"
#include "common/Flags.h"
#include "common/JsonUtil.h"
#include "common/ParamExtractor.h"
#include "common/YamlUtil.h"
#include "plugin/PluginRegistry.h"

DEFINE_FLAG_BOOL(enable_env_ref_in_config, "enable environment variable reference replacement in configuration", false);

using namespace std;

namespace logtail {

static string UnescapeDollar(string::const_iterator beginIt, string::const_iterator endIt) {
    string outStr;
    string::const_iterator lastMatchEnd = beginIt;
    static boost::regex reg(R"(\$\$|\$})");
    boost::regex_iterator<string::const_iterator> it{beginIt, endIt, reg};
    boost::regex_iterator<string::const_iterator> end;
    for (; it != end; ++it) {
        outStr.append(lastMatchEnd, (*it)[0].first); // original part
        outStr.append((*it)[0].first + 1, (*it)[0].second); // skip $ char
        lastMatchEnd = (*it)[0].second;
    }
    outStr.append(lastMatchEnd, endIt); // original part
    return outStr;
}

static bool ReplaceEnvVarRefInStr(const string& inStr, string& outStr) {
    string::const_iterator lastMatchEnd = inStr.begin();
    static boost::regex reg(R"((?<!\$)\${([\w]+)(:(.*?))?(?<!\$)})");
    boost::regex_iterator<string::const_iterator> it{inStr.begin(), inStr.end(), reg};
    boost::regex_iterator<string::const_iterator> end;
    if (it == end) {
        outStr.append(UnescapeDollar(lastMatchEnd, inStr.end())); // original part
        return false;
    }
    for (; it != end; ++it) {
        outStr.append(UnescapeDollar(lastMatchEnd, (*it)[0].first)); // original part
        char* env = getenv((*it)[1].str().c_str());
        if (env != NULL) // replace to enviroment variable
        {
            outStr.append(env);
        } else if ((*it).size() == 4) // replace to default value
        {
            outStr.append((*it)[3].first, (*it)[3].second);
        }
        // else replace to empty string (do nothing)
        lastMatchEnd = (*it)[0].second;
    }
    outStr.append(UnescapeDollar(lastMatchEnd, inStr.end())); // original part
    return true;
}

static void ReplaceEnvVarRef(Json::Value& value, bool& res) {
    if (value.isString()) {
        string outStr;
        if (ReplaceEnvVarRefInStr(value.asString(), outStr)) {
            res = true;
        }
        Json::Value tempValue{outStr};
        value.swapPayload(tempValue);
    } else if (value.isArray()) {
        Json::ValueIterator it = value.begin();
        Json::ValueIterator end = value.end();
        for (; it != end; ++it) {
            ReplaceEnvVarRef(*it, res);
        }
    } else if (value.isObject()) {
        Json::ValueIterator it = value.begin();
        Json::ValueIterator end = value.end();
        for (; it != end; ++it) {
            ReplaceEnvVarRef(*it, res);
        }
    }
}

bool PipelineConfig::Parse() {
    if (BOOL_FLAG(enable_env_ref_in_config)) {
        if (ReplaceEnvVar()) {
            LOG_INFO(sLogger, ("env vars in config are replaced, config", mDetail->toStyledString())("config", mName));
        }
    }

    string key, errorMsg;
    const Json::Value* itr = nullptr;
    LogtailAlarm& alarm = *LogtailAlarm::GetInstance();
    // to send alarm and init MetricsRecord, project, logstore and region should be extracted first.
    key = "flushers";
    itr = mDetail->find(key.c_str(), key.c_str() + key.size());
    if (itr && itr->isArray()) {
        for (Json::Value::ArrayIndex i = 0; i < itr->size(); ++i) {
            const Json::Value& plugin = (*itr)[i];
            if (plugin.isObject()) {
                key = "Type";
                const Json::Value* it = plugin.find(key.c_str(), key.c_str() + key.size());
                if (it && it->isString() && it->asString() == "flusher_sls") {
                    GetMandatoryStringParam(plugin, "Project", mProject, errorMsg);
                    GetMandatoryStringParam(plugin, "Logstore", mLogstore, errorMsg);
                    GetMandatoryStringParam(plugin, "Region", mRegion, errorMsg);
                }
            }
        }
    }

    if (!GetOptionalUIntParam(*mDetail, "createTime", mCreateTime, errorMsg)) {
        PARAM_WARNING_DEFAULT(sLogger, alarm, errorMsg, mCreateTime, noModule, mName, mProject, mLogstore, mRegion);
    }

    key = "global";
    itr = mDetail->find(key.c_str(), key.c_str() + key.size());
    if (itr) {
        if (!itr->isObject()) {
            PARAM_ERROR_RETURN(
                sLogger, alarm, "global module is not of type object", noModule, mName, mProject, mLogstore, mRegion);
        }
        mGlobal = itr;
    }

    // inputs, processors and flushers module must be parsed first and parsed by order, since aggregators and
    // extensions module parsing will rely on their results.
    bool hasObserverInput = false;
    bool hasFileInput = false;
#ifdef __ENTERPRISE__
    bool hasStreamInput = false;
#endif
    key = "inputs";
    itr = mDetail->find(key.c_str(), key.c_str() + key.size());
    if (!itr) {
        PARAM_ERROR_RETURN(
            sLogger, alarm, "mandatory inputs module is missing", noModule, mName, mProject, mLogstore, mRegion);
    }
    if (!itr->isArray()) {
        PARAM_ERROR_RETURN(sLogger,
                           alarm,
                           "mandatory inputs module is not of type array",
                           noModule,
                           mName,
                           mProject,
                           mLogstore,
                           mRegion);
    }
    if (itr->empty()) {
        PARAM_ERROR_RETURN(
            sLogger, alarm, "mandatory inputs module has no plugin", noModule, mName, mProject, mLogstore, mRegion);
    }
    for (Json::Value::ArrayIndex i = 0; i < itr->size(); ++i) {
        const Json::Value& plugin = (*itr)[i];
        if (!plugin.isObject()) {
            PARAM_ERROR_RETURN(sLogger,
                               alarm,
                               "param inputs[" + ToString(i) + "] is not of type object",
                               noModule,
                               mName,
                               mProject,
                               mLogstore,
                               mRegion);
        }
        key = "Type";
        const Json::Value* it = plugin.find(key.c_str(), key.c_str() + key.size());
        if (it == nullptr) {
            PARAM_ERROR_RETURN(sLogger,
                               alarm,
                               "param inputs[" + ToString(i) + "].Type is missing",
                               noModule,
                               mName,
                               mProject,
                               mLogstore,
                               mRegion);
        }
        if (!it->isString()) {
            PARAM_ERROR_RETURN(sLogger,
                               alarm,
                               "param inputs[" + ToString(i) + "].Type is not of type string",
                               noModule,
                               mName,
                               mProject,
                               mLogstore,
                               mRegion);
        }
        const string pluginType = it->asString();
        if (i == 0) {
            if (PluginRegistry::GetInstance()->IsValidGoPlugin(pluginType)) {
                mHasGoInput = true;
            } else if (PluginRegistry::GetInstance()->IsValidNativeInputPlugin(pluginType)) {
                mHasNativeInput = true;
            } else {
                PARAM_ERROR_RETURN(
                    sLogger, alarm, "unsupported input plugin", pluginType, mName, mProject, mLogstore, mRegion);
            }
        } else {
            if (mHasGoInput) {
                if (PluginRegistry::GetInstance()->IsValidNativeInputPlugin(pluginType)) {
                    PARAM_ERROR_RETURN(sLogger,
                                       alarm,
                                       "native and extended input plugins coexist",
                                       noModule,
                                       mName,
                                       mProject,
                                       mLogstore,
                                       mRegion);
                } else if (!PluginRegistry::GetInstance()->IsValidGoPlugin(pluginType)) {
                    PARAM_ERROR_RETURN(
                        sLogger, alarm, "unsupported input plugin", pluginType, mName, mProject, mLogstore, mRegion);
                }
            } else {
                if (PluginRegistry::GetInstance()->IsValidGoPlugin(pluginType)) {
                    PARAM_ERROR_RETURN(sLogger,
                                       alarm,
                                       "native and extended input plugins coexist",
                                       noModule,
                                       mName,
                                       mProject,
                                       mLogstore,
                                       mRegion);
                } else if (!PluginRegistry::GetInstance()->IsValidNativeInputPlugin(pluginType)) {
                    PARAM_ERROR_RETURN(
                        sLogger, alarm, "unsupported input plugin", pluginType, mName, mProject, mLogstore, mRegion);
                }
            }
        }
        mInputs.push_back(&plugin);
        // TODO: remove these special restrictions
        if (pluginType == "input_observer_network") {
            hasObserverInput = true;
        } else if (pluginType == "input_file" || pluginType == "input_container_stdio") {
            hasFileInput = true;
#ifdef __ENTERPRISE__
        } else if (pluginType == "input_stream") {
            if (!AppConfig::GetInstance()->GetOpenStreamLog()) {
                PARAM_ERROR_RETURN(
                    sLogger, alarm, "stream log is not enabled", noModule, mName, mProject, mLogstore, mRegion);
            }
            hasStreamInput = true;
#endif
        }
    }
    // TODO: remove these special restrictions
    bool hasSpecialInput = hasObserverInput || hasFileInput;
#ifdef __ENTERPRISE__
    hasSpecialInput = hasSpecialInput || hasStreamInput;
#endif
    if (hasSpecialInput && (*mDetail)["inputs"].size() > 1) {
        PARAM_ERROR_RETURN(sLogger,
                           alarm,
                           "more than 1 input_file or input_container_stdio plugin is given",
                           noModule,
                           mName,
                           mProject,
                           mLogstore,
                           mRegion);
    }

    key = "processors";
    itr = mDetail->find(key.c_str(), key.c_str() + key.size());
    if (itr) {
        if (!itr->isArray()) {
            PARAM_ERROR_RETURN(sLogger,
                               alarm,
                               "processors module is not of type array",
                               noModule,
                               mName,
                               mProject,
                               mLogstore,
                               mRegion);
        }
#ifdef __ENTERPRISE__
        // TODO: remove these special restrictions
        if (hasStreamInput && !itr->empty()) {
            PARAM_ERROR_RETURN(sLogger,
                               alarm,
                               "processor plugins coexist with input_stream",
                               noModule,
                               mName,
                               mProject,
                               mLogstore,
                               mRegion);
        }
#endif
        bool isCurrentPluginNative = true;
        for (Json::Value::ArrayIndex i = 0; i < itr->size(); ++i) {
            const Json::Value& plugin = (*itr)[i];
            if (!plugin.isObject()) {
                PARAM_ERROR_RETURN(sLogger,
                                   alarm,
                                   "param processors[" + ToString(i) + "] is not of type object",
                                   noModule,
                                   mName,
                                   mProject,
                                   mLogstore,
                                   mRegion);
            }
            key = "Type";
            const Json::Value* it = plugin.find(key.c_str(), key.c_str() + key.size());
            if (it == nullptr) {
                PARAM_ERROR_RETURN(sLogger,
                                   alarm,
                                   "param processors[" + ToString(i) + "].Type is missing",
                                   noModule,
                                   mName,
                                   mProject,
                                   mLogstore,
                                   mRegion);
            }
            if (!it->isString()) {
                PARAM_ERROR_RETURN(sLogger,
                                   alarm,
                                   "param processors[" + ToString(i) + "].Type is not of type string",
                                   noModule,
                                   mName,
                                   mProject,
                                   mLogstore,
                                   mRegion);
            }
            const string pluginType = it->asString();
            if (mHasGoInput) {
                if (PluginRegistry::GetInstance()->IsValidNativeProcessorPlugin(pluginType)) {
                    PARAM_ERROR_RETURN(sLogger,
                                       alarm,
                                       "native processor plugins coexist with extended input plugins",
                                       noModule,
                                       mName,
                                       mProject,
                                       mLogstore,
                                       mRegion);
                } else if (PluginRegistry::GetInstance()->IsValidGoPlugin(pluginType)) {
                    mHasGoProcessor = true;
                } else {
                    PARAM_ERROR_RETURN(sLogger,
                                       alarm,
                                       "unsupported processor plugin",
                                       pluginType,
                                       mName,
                                       mProject,
                                       mLogstore,
                                       mRegion);
                }
            } else {
                if (isCurrentPluginNative) {
                    if (PluginRegistry::GetInstance()->IsValidGoPlugin(pluginType)) {
                        // TODO: remove these special restrictions
                        if (!hasObserverInput && !hasFileInput) {
                            PARAM_ERROR_RETURN(sLogger,
                                               alarm,
                                               "extended processor plugins coexist with native input plugins other "
                                               "than input_file or input_container_stdio",
                                               noModule,
                                               mName,
                                               mProject,
                                               mLogstore,
                                               mRegion);
                        }
                        isCurrentPluginNative = false;
                        mHasGoProcessor = true;
                    } else if (!PluginRegistry::GetInstance()->IsValidNativeProcessorPlugin(pluginType)) {
                        PARAM_ERROR_RETURN(sLogger,
                                           alarm,
                                           "unsupported processor plugin",
                                           pluginType,
                                           mName,
                                           mProject,
                                           mLogstore,
                                           mRegion);
                    } else if (pluginType == "processor_spl") {
                        if (i != 0 || itr->size() != 1) {
                            PARAM_ERROR_RETURN(sLogger,
                                               alarm,
                                               "native processor plugins coexist with spl processor",
                                               noModule,
                                               mName,
                                               mProject,
                                               mLogstore,
                                               mRegion);
                        }
                    } else {
                        // TODO: remove these special restrictions
                        if (hasObserverInput) {
                            PARAM_ERROR_RETURN(sLogger,
                                               alarm,
                                               "native processor plugins coexist with input_observer_network",
                                               noModule,
                                               mName,
                                               mProject,
                                               mLogstore,
                                               mRegion);
                        }
                        mHasNativeProcessor = true;
                    }
                } else {
                    if (PluginRegistry::GetInstance()->IsValidNativeProcessorPlugin(pluginType)) {
                        PARAM_ERROR_RETURN(sLogger,
                                           alarm,
                                           "native processor plugin comes after extended processor plugin",
                                           pluginType,
                                           mName,
                                           mProject,
                                           mLogstore,
                                           mRegion);
                    } else if (PluginRegistry::GetInstance()->IsValidGoPlugin(pluginType)) {
                        mHasGoProcessor = true;
                    } else {
                        PARAM_ERROR_RETURN(sLogger,
                                           alarm,
                                           "unsupported processor plugin",
                                           pluginType,
                                           mName,
                                           mProject,
                                           mLogstore,
                                           mRegion);
                    }
                }
            }
            mProcessors.push_back(&plugin);
            if (i == 0) {
                if (pluginType == "processor_parse_json_native" || pluginType == "processor_json") {
                    mIsFirstProcessorJson = true;
                }
            }
        }
    }

    bool hasFlusherSLS = false;
    uint32_t nativeFlusherCnt = 0;
    key = "flushers";
    itr = mDetail->find(key.c_str(), key.c_str() + key.size());
    if (!itr) {
        PARAM_ERROR_RETURN(
            sLogger, alarm, "mandatory flushers module is missing", noModule, mName, mProject, mLogstore, mRegion);
    }
    if (!itr->isArray()) {
        PARAM_ERROR_RETURN(sLogger,
                           alarm,
                           "mandatory flushers module is not of type array",
                           noModule,
                           mName,
                           mProject,
                           mLogstore,
                           mRegion);
    }
    if (itr->empty()) {
        PARAM_ERROR_RETURN(
            sLogger, alarm, "mandatory flushers module has no plugin", noModule, mName, mProject, mLogstore, mRegion);
    }
    for (Json::Value::ArrayIndex i = 0; i < itr->size(); ++i) {
        const Json::Value& plugin = (*itr)[i];
        if (!plugin.isObject()) {
            PARAM_ERROR_RETURN(sLogger,
                               alarm,
                               "param flushers[" + ToString(i) + "] is not of type object",
                               noModule,
                               mName,
                               mProject,
                               mLogstore,
                               mRegion);
        }
        key = "Type";
        const Json::Value* it = plugin.find(key.c_str(), key.c_str() + key.size());
        if (it == nullptr) {
            PARAM_ERROR_RETURN(sLogger,
                               alarm,
                               "param flushers[" + ToString(i) + "].Type is missing",
                               noModule,
                               mName,
                               mProject,
                               mLogstore,
                               mRegion);
        }
        if (!it->isString()) {
            PARAM_ERROR_RETURN(sLogger,
                               alarm,
                               "param flushers[" + ToString(i) + "].Type is not of type string",
                               noModule,
                               mName,
                               mProject,
                               mLogstore,
                               mRegion);
        }
        const string pluginType = it->asString();
        if (PluginRegistry::GetInstance()->IsValidGoPlugin(pluginType)) {
            // TODO: remove these special restrictions
            if (mHasNativeInput && !hasFileInput && !hasObserverInput) {
                PARAM_ERROR_RETURN(sLogger,
                                   alarm,
                                   "extended flusher plugins coexist with native input plugins other than "
                                   "input_file or input_container_stdio",
                                   noModule,
                                   mName,
                                   mProject,
                                   mLogstore,
                                   mRegion);
            }
            mHasGoFlusher = true;
        } else if (PluginRegistry::GetInstance()->IsValidNativeFlusherPlugin(pluginType)) {
            mHasNativeFlusher = true;
            // TODO: remove these special restrictions
            ++nativeFlusherCnt;
            if (pluginType == "flusher_sls") {
                hasFlusherSLS = true;
            }
        } else {
            PARAM_ERROR_RETURN(
                sLogger, alarm, "unsupported flusher plugin", pluginType, mName, mProject, mLogstore, mRegion);
        }
#ifdef __ENTERPRISE__
        // TODO: remove these special restrictions
        if (hasStreamInput && pluginType != "flusher_sls") {
            PARAM_ERROR_RETURN(sLogger,
                               alarm,
                               "flusher plugins other than flusher_sls coexist with input_stream",
                               noModule,
                               mName,
                               mProject,
                               mLogstore,
                               mRegion);
        }
#endif
        mFlushers.push_back(&plugin);
    }
    // TODO: remove these special restrictions
    if (mHasGoFlusher && nativeFlusherCnt > 1) {
        PARAM_ERROR_RETURN(sLogger,
                           alarm,
                           "more than 1 native flusher plugins coexist with extended flusher plugins",
                           noModule,
                           mName,
                           mProject,
                           mLogstore,
                           mRegion);
    }
    if (mHasGoFlusher && nativeFlusherCnt == 1 && !hasFlusherSLS) {
        PARAM_ERROR_RETURN(sLogger,
                           alarm,
                           "native flusher plugins other than flusher_sls coexist with extended flusher plugins",
                           noModule,
                           mName,
                           mProject,
                           mLogstore,
                           mRegion);
    }

    key = "Match";
    for (size_t i = 0; i < mFlushers.size(); ++i) {
        auto itr = mFlushers[i]->find(key.c_str(), key.c_str() + key.size());
        if (itr) {
            if (IsFlushingThroughGoPipelineExisted()) {
                PARAM_ERROR_RETURN(sLogger,
                                   alarm,
                                   "route found in non-native flushing mode",
                                   noModule,
                                   mName,
                                   mProject,
                                   mLogstore,
                                   mRegion);
            }
            mRouter.emplace_back(i, itr);
        } else {
            mRouter.emplace_back(i, nullptr);
        }
    }

    key = "aggregators";
    itr = mDetail->find(key.c_str(), key.c_str() + key.size());
    if (itr) {
        if (!IsFlushingThroughGoPipelineExisted()) {
            PARAM_ERROR_RETURN(sLogger,
                               alarm,
                               "aggregator plugins exist in native flushing mode",
                               noModule,
                               mName,
                               mProject,
                               mLogstore,
                               mRegion);
        }
        if (!itr->isArray()) {
            PARAM_ERROR_RETURN(sLogger,
                               alarm,
                               "aggregators module is not of type array",
                               noModule,
                               mName,
                               mProject,
                               mLogstore,
                               mRegion);
        }
        if (itr->size() != 1) {
            PARAM_ERROR_RETURN(
                sLogger, alarm, "more than 1 aggregator is given", noModule, mName, mProject, mLogstore, mRegion);
        }
        for (Json::Value::ArrayIndex i = 0; i < itr->size(); ++i) {
            const Json::Value& plugin = (*itr)[i];
            if (!plugin.isObject()) {
                PARAM_ERROR_RETURN(sLogger,
                                   alarm,
                                   "param aggregators[" + ToString(i) + "] is not of type object",
                                   noModule,
                                   mName,
                                   mProject,
                                   mLogstore,
                                   mRegion);
            }
            key = "Type";
            const Json::Value* it = plugin.find(key.c_str(), key.c_str() + key.size());
            if (it == nullptr) {
                PARAM_ERROR_RETURN(sLogger,
                                   alarm,
                                   "param aggregators[" + ToString(i) + "].Type is missing",
                                   noModule,
                                   mName,
                                   mProject,
                                   mLogstore,
                                   mRegion);
            }
            if (!it->isString()) {
                PARAM_ERROR_RETURN(sLogger,
                                   alarm,
                                   "param aggregators[" + ToString(i) + "].Type is not of type string",
                                   noModule,
                                   mName,
                                   mProject,
                                   mLogstore,
                                   mRegion);
            }
            const string pluginType = it->asString();
            if (!PluginRegistry::GetInstance()->IsValidGoPlugin(pluginType)) {
                PARAM_ERROR_RETURN(
                    sLogger, alarm, "unsupported aggregator plugin", pluginType, mName, mProject, mLogstore, mRegion);
            }
            mAggregators.push_back(&plugin);
        }
    }

    key = "extensions";
    itr = mDetail->find(key.c_str(), key.c_str() + key.size());
    if (itr) {
        if (!HasGoPlugin()) {
            PARAM_ERROR_RETURN(sLogger,
                               alarm,
                               "extension plugins exist when no extended plugin is given",
                               noModule,
                               mName,
                               mProject,
                               mLogstore,
                               mRegion);
        }
        if (!itr->isArray()) {
            PARAM_ERROR_RETURN(sLogger,
                               alarm,
                               "extensions module is not of type array",
                               noModule,
                               mName,
                               mProject,
                               mLogstore,
                               mRegion);
        }
        for (Json::Value::ArrayIndex i = 0; i < itr->size(); ++i) {
            const Json::Value& plugin = (*itr)[i];
            if (!plugin.isObject()) {
                PARAM_ERROR_RETURN(sLogger,
                                   alarm,
                                   "param extensions[" + ToString(i) + "] is not of type object",
                                   noModule,
                                   mName,
                                   mProject,
                                   mLogstore,
                                   mRegion);
            }
            key = "Type";
            const Json::Value* it = plugin.find(key.c_str(), key.c_str() + key.size());
            if (it == nullptr) {
                PARAM_ERROR_RETURN(sLogger,
                                   alarm,
                                   "param extensions[" + ToString(i) + "].Type is missing",
                                   noModule,
                                   mName,
                                   mProject,
                                   mLogstore,
                                   mRegion);
            }
            if (!it->isString()) {
                PARAM_ERROR_RETURN(sLogger,
                                   alarm,
                                   "param extensions[" + ToString(i) + "].Type is not of type string",
                                   noModule,
                                   mName,
                                   mProject,
                                   mLogstore,
                                   mRegion);
            }
            const string pluginType = it->asString();
            if (!PluginRegistry::GetInstance()->IsValidGoPlugin(pluginType)) {
                PARAM_ERROR_RETURN(
                    sLogger, alarm, "unsupported extension plugin", pluginType, mName, mProject, mLogstore, mRegion);
            }
            mExtensions.push_back(&plugin);
        }
    }

    return true;
}

bool PipelineConfig::ReplaceEnvVar() {
    bool res = false;
    ReplaceEnvVarRef(*mDetail, res);
    return res;
}

bool LoadConfigDetailFromFile(const filesystem::path& filepath, Json::Value& detail) {
    const string& ext = filepath.extension().string();
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

} // namespace logtail
