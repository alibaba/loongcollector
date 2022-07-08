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

#include "ConfigYamlToJson.h"
#include "logger/Logger.h"
#include <iostream>

using namespace logtail;

namespace logtail {

const std::string PLUGIN_CATEGORY_INPUTS = "inputs";
const std::string PLUGIN_CATEGORY_PROCESSORS = "processors";
const std::string PLUGIN_CATEGORY_FLUSHERS = "flushers";

const std::string INPUT_FILE_LOG = "file_log";

const std::string PROCESSOR_REGEX_ACCELERATE = "processor_regex_accelerate";
const std::string PROCESSOR_JSON_ACCELERATE = "processor_json_accelerate";
const std::string PROCESSOR_DELIMITER_ACCELERATE = "processor_delimiter_accelerate";

const std::string PROCESSOR_SPLIT_LINE_LOG_USING_SEP = "processor_split_log_string";
const std::string PROCESSOR_SPLIT_LINE_LOG_USING_REG = "processor_split_log_regex";

ConfigYamlToJson::ConfigYamlToJson() {
    mFileConfigMap["logType"] = "log_type";
    mFileConfigMap["LogPath"] = "log_path";
    mFileConfigMap["FilePattern"] = "file_pattern";
    mFileConfigMap["FileEncoding"] = "file_encoding";
    mFileConfigMap["LogBeginRegex"] = "log_begin_reg";
    mFileConfigMap["TailExisted"] = "tail_existed";
    mFileConfigMap["Preserve"] = "preserve";
    mFileConfigMap["PreserveDepth"] = "preserve_depth";
    mFileConfigMap["MaxDepth"] = "max_depth";
    mFileConfigMap["TopicFormat"] = "topic_format";
    mFileConfigMap["DiscardNonUtf8"] = "discard_none_utf8";
    mFileConfigMap["DiscardUnmatch"] = "discard_unmatch";
    mFileConfigMap["DelaySkipBytes"] = "delay_skip_bytes";
    mFileConfigMap["DockerFile"] = "docker_file";
    mFileConfigMap["DockerIncludeLabel"] = "docker_include_label";
    mFileConfigMap["DockerExcludeLabel"] = "docker_exclude_label";
    mFileConfigMap["DockerIncludeEnv"] = "docker_include_env";
    mFileConfigMap["DockerExcludeEnv"] = "docker_exclude_env";
    mFileConfigMap["CustomizedFields"] = "customizedFields";

    mFileConfigMap["Regex"] = "regex";
    mFileConfigMap["Keys"] = "keys";
    mFileConfigMap["LocalStorage"] = "local_storage";
    mFileConfigMap["EnableTag"] = "enable_tag";
    mFileConfigMap["EnableRawLog"] = "raw_log";
    mFileConfigMap["FilterRegex"] = "filter_regs";
    mFileConfigMap["FilterKey"] = "filter_keys";
    mFileConfigMap["AdjustTimezone"] = "tz_adjust";
    mFileConfigMap["LogTimezone"] = "log_tz";
    mFileConfigMap["DelayAlarmBytes"] = "delay_alarm_bytes";
    mFileConfigMap["SensitiveKeys"] = "sensitive_keys";
    mFileConfigMap["MergeType"] = "merge_type";
    mFileConfigMap["Priority"] = "priority";

    mFileConfigMap["ProjectName"] = "project_name";
    mFileConfigMap["LogstoreName"] = "category";
    mFileConfigMap["Endpoint"] = "defaultEndpoint";
    mFileConfigMap["Region"] = "region";
    mFileConfigMap["ShardHashKey"] = "shard_hash_key";
    mFileConfigMap["MaxSendRate"] = "max_send_rate";

    mFilePluginToLogTypeMap[INPUT_FILE_LOG] = "common_reg_log";
    mFilePluginToLogTypeMap[PROCESSOR_REGEX_ACCELERATE] = "common_reg_log";
    mFilePluginToLogTypeMap[PROCESSOR_JSON_ACCELERATE] = "json_log";
    mFilePluginToLogTypeMap[PROCESSOR_DELIMITER_ACCELERATE] = "delimiter_log";
}

string ConfigYamlToJson::GetTransforKey(const string yamlKey) {
    unordered_map<string, string>::iterator iter = mFileConfigMap.find(yamlKey);
    if (iter != mFileConfigMap.end())
        return iter->second;
    return "";
}

Json::Value ConfigYamlToJson::ParseScalar(const YAML::Node& node) {
    int i;
    if (YAML::convert<int>::decode(node, i))
        return i;

    double d;
    if (YAML::convert<double>::decode(node, d))
        return d;

    bool b;
    if (YAML::convert<bool>::decode(node, b))
        return b;

    string s;
    if (YAML::convert<string>::decode(node, s))
        return s;

    return nullptr;
}

Json::Value ConfigYamlToJson::ChangeYamlToJson(const YAML::Node& rootNode) {
    Json::Value resultJson;

    switch (rootNode.Type()) {
        case YAML::NodeType::Null:
            break;

        case YAML::NodeType::Scalar:
            return ParseScalar(rootNode);

        case YAML::NodeType::Sequence: {
            int i = 0;
            for (auto&& node : rootNode) {
                resultJson[i] = ChangeYamlToJson(node);
                i++;
            }
            break;
        }

        case YAML::NodeType::Map: {
            for (auto&& it : rootNode) {
                resultJson[it.first.as<std::string>()] = ChangeYamlToJson(it.second);
            }
            break;
        }

        default:
            break;
    }

    return resultJson;
}

bool ConfigYamlToJson::GenerateLocalJsonConfig(const string configName,
                                               const YAML::Node& yamlConfig,
                                               Json::Value& userLocalJsonConfig) {
    try {
        WorkMode workMode;
        if (!CheckPluginConfig(configName, yamlConfig, workMode)) {
            return false;
        }

        Json::Value pluginJsonConfig;
        Json::Value userJsonConfig;
        if (!GenerateLocalJsonConfigForPluginCategory(
                configName, workMode, PLUGIN_CATEGORY_INPUTS, yamlConfig, pluginJsonConfig, userJsonConfig)) {
            return false;
        }
        if (!GenerateLocalJsonConfigForPluginCategory(
                configName, workMode, PLUGIN_CATEGORY_PROCESSORS, yamlConfig, pluginJsonConfig, userJsonConfig)) {
            return false;
        }
        if (!GenerateLocalJsonConfigForPluginCategory(
                configName, workMode, PLUGIN_CATEGORY_FLUSHERS, yamlConfig, pluginJsonConfig, userJsonConfig)) {
            return false;
        }

        FillupDefalutUserJsonConfig(workMode, userJsonConfig);
        userJsonConfig["plugin"] = pluginJsonConfig;
        userJsonConfig["log_type"] = workMode.mLogType;
        if (yamlConfig["enable"])
            userJsonConfig["enable"] = yamlConfig["enable"].as<bool>();
        else
            userJsonConfig["enable"] = false;
        userLocalJsonConfig["metrics"]["config#" + configName] = userJsonConfig;

        LOG_INFO(sLogger,
                 ("Trans yaml to json", "success")("config_name", configName)("is_file_mode", workMode.mIsFileMode)(
                     "input_plugin_type", workMode.mInputPluginType)("has_accelerate_processor",
                                                                     workMode.mHasAccelerateProcessor)(
                     "accelerate_processor_plugin_type", workMode.mAccelerateProcessorPluginType)(
                     "log_split_processor", workMode.mLogSplitProcessorPluginType)("logtype", workMode.mLogType)(
                     "user_local_json_config", userLocalJsonConfig.toStyledString()));
    } catch (const YAML::Exception& e) {
        LOG_WARNING(sLogger,
                    ("GenerateLocalJsonConfig", "parse yaml failed.")("config_name", configName)("error", e.what()));
        return false;
    } catch (const exception& e) {
        LOG_WARNING(sLogger,
                    ("GenerateLocalJsonConfig", "parse failed.")("config_name", configName)("error", e.what()));
        return false;
    } catch (...) {
        LOG_WARNING(sLogger,
                    ("GenerateLocalJsonConfig", "parse failed.")("config_name", configName)("error", "unkown reason"));
        return false;
    }

    return true;
}

bool ConfigYamlToJson::CheckPluginConfig(const string configName, const YAML::Node& yamlConfig, WorkMode& workMode) {
    unordered_map<string, PluginInfo> inputPluginsInfo;
    unordered_map<string, PluginInfo> processorPluginsInfo;
    unordered_map<string, PluginInfo> flusherPluginsInfo;

    workMode.reset();

    if (!GeneratePluginStatistics(PLUGIN_CATEGORY_INPUTS, yamlConfig, inputPluginsInfo)) {
        LOG_ERROR(sLogger, ("CheckPluginConfig failed", "inputs have invalid config.")("config_name", configName));
        return false;
    }
    if (!GeneratePluginStatistics(PLUGIN_CATEGORY_PROCESSORS, yamlConfig, processorPluginsInfo)) {
        LOG_ERROR(sLogger, ("CheckPluginConfig failed", "processors have invalid config.")("config_name", configName));
        return false;
    }
    if (!GeneratePluginStatistics(PLUGIN_CATEGORY_FLUSHERS, yamlConfig, flusherPluginsInfo)) {
        LOG_ERROR(sLogger, ("CheckPluginConfig failed", "flushers have invalid config.")("config_name", configName));
        return false;
    }

    if (inputPluginsInfo.size() != 1) {
        LOG_ERROR(sLogger,
                  ("CheckPluginConfig failed", "inputPluginsInfo size is not 1.")("config_name", configName)(
                      "input_plugin_size", inputPluginsInfo.size()));
        return false;
    }

    workMode.mInputPluginType = inputPluginsInfo.begin()->first;

    workMode.mHasAccelerateProcessor = false;
    for (unordered_map<string, PluginInfo>::iterator iter = processorPluginsInfo.begin();
         iter != processorPluginsInfo.end();
         iter++) {
        string processorPluginType = iter->first;
        unordered_map<string, string>::iterator accProcessorIter = mFilePluginToLogTypeMap.find(processorPluginType);
        if (accProcessorIter != mFilePluginToLogTypeMap.end()) {
            workMode.mHasAccelerateProcessor = true;
            workMode.mAccelerateProcessorPluginType = accProcessorIter->first;
            if (workMode.mInputPluginType.compare(INPUT_FILE_LOG)) {
                LOG_ERROR(sLogger,
                          ("CheckPluginConfig failed", "accelerateProcessor must be used with file_log plugin.")(
                              "config_name", configName)("input_plugin_type", workMode.mInputPluginType)(
                              "accelerate_processor", workMode.mAccelerateProcessorPluginType));
                return false;
            }
            break;
        } else {
            if (0 == workMode.mLogSplitProcessorPluginType.size()
                && (0 == processorPluginType.compare(PROCESSOR_SPLIT_LINE_LOG_USING_SEP)
                    || 0 == processorPluginType.compare(PROCESSOR_SPLIT_LINE_LOG_USING_REG))) {
                workMode.mLogSplitProcessorPluginType = processorPluginType;
                if (iter->second.mFirstPos != 0) {
                    LOG_ERROR(sLogger,
                              ("CheckPluginConfig failed",
                               "processor_split_log_string and processor_split_log_regex must be first processor.")(
                                  "config_name", configName)("processor_plugin_type", processorPluginType)(
                                  "first_postion", iter->second.mFirstPos)("times", iter->second.mTimes));
                    return false;
                }
            }
        }
    }

    if (0 == workMode.mInputPluginType.compare(INPUT_FILE_LOG)) {
        workMode.mIsFileMode = true;
        if (workMode.mHasAccelerateProcessor) {
            workMode.mLogType = mFilePluginToLogTypeMap[workMode.mAccelerateProcessorPluginType];
        } else {
            workMode.mLogType = "common_reg_log";
        }
    } else {
        workMode.mIsFileMode = false;
        workMode.mLogType = "plugin";
    }

    if (!workMode.mIsFileMode && workMode.mHasAccelerateProcessor) {
        LOG_ERROR(
            sLogger,
            ("CheckPluginConfig failed", "not file mode but has accelerate processor.")("config_name", configName));
        return false;
    }
    if (workMode.mIsFileMode && workMode.mHasAccelerateProcessor && processorPluginsInfo.size() != 1) {
        LOG_ERROR(sLogger,
                  ("CheckPluginConfig failed",
                   "when use accelerate processor, can not use other processors.")("config_name", configName));
        return false;
    }

    if (flusherPluginsInfo.size() == 0) {
        LOG_ERROR(sLogger, ("CheckPluginConfig failed", "do not config flusher.")("config_name", configName));
        return false;
    }

    LOG_DEBUG(sLogger,
              ("CheckPluginConfig", "success")("is_file_mode", workMode.mIsFileMode)(
                  "input_plugin_type", workMode.mInputPluginType)("has_accelerate_processor",
                                                                  workMode.mHasAccelerateProcessor)(
                  "accelerate_processor_plugin_type", workMode.mAccelerateProcessorPluginType)(
                  "log_split_processor", workMode.mLogSplitProcessorPluginType)("logtype", workMode.mLogType));

    return true;
}

bool ConfigYamlToJson::GeneratePluginStatistics(const string pluginCategory,
                                                const YAML::Node& yamlConfig,
                                                unordered_map<string, PluginInfo>& pluginInfos) {
    Json::Value plugins;
    if (yamlConfig[pluginCategory]) {
        const YAML::Node& pluginsYaml = yamlConfig[pluginCategory];
        if (pluginsYaml.IsSequence()) {
            for (uint32_t i = 0; i < pluginsYaml.size(); i++) {
                if (!pluginsYaml[i]["Type"].IsDefined()) {
                    return false;
                }

                string pluginType = pluginsYaml[i]["Type"].as<std::string>();
                PluginInfo info;
                info.mTimes = 1;
                info.mFirstPos = i;
                unordered_map<string, PluginInfo>::iterator iter = pluginInfos.find(pluginType);
                if (iter != pluginInfos.end())
                    iter->second.mTimes++;
                else
                    pluginInfos[pluginType] = info;
            }
        }
    }

    return true;
}

bool ConfigYamlToJson::FillupMustMultiLinesSplitProcessor(const WorkMode& workMode, Json::Value& splitProcessor) {
    if (!workMode.mIsFileMode || workMode.mHasAccelerateProcessor) {
        return false;
    }

    if (0 != workMode.mLogSplitProcessorPluginType.size()) {
        return false;
    }

    Json::Value pluginDetails;
    if (0 == workMode.mInputPluginType.compare(INPUT_FILE_LOG)) {
        pluginDetails["SplitKey"] = "content";
        splitProcessor["detail"] = pluginDetails;
        splitProcessor["type"] = PROCESSOR_SPLIT_LINE_LOG_USING_SEP;
    }

    return true;
}

bool ConfigYamlToJson::GenerateLocalJsonConfigForPluginCategory(const string configName,
                                                                const WorkMode& workMode,
                                                                const string pluginCategory,
                                                                const YAML::Node& yamlConfig,
                                                                Json::Value& pluginsJsonConfig,
                                                                Json::Value& userJsonConfig) {
    Json::Value plugins;

    if (0 == pluginCategory.compare(PLUGIN_CATEGORY_PROCESSORS)) {
        Json::Value splitProcessor;
        if (FillupMustMultiLinesSplitProcessor(workMode, splitProcessor))
            plugins.append(splitProcessor);
    }

    if (yamlConfig[pluginCategory]) {
        const YAML::Node& pluginsYaml = yamlConfig[pluginCategory];
        if (pluginsYaml.IsSequence()) {
            for (YAML::const_iterator it = pluginsYaml.begin(); it != pluginsYaml.end(); ++it) {
                const YAML::Node& pluginYamlNode = *it;
                Json::Value pluginJsonConfig;
                if (GenerateLocalJsonConfigForFileMode(pluginYamlNode, userJsonConfig)) {
                } else if (GenerateLocalJsonConfigForSLSFulsher(pluginYamlNode, pluginJsonConfig, userJsonConfig)) {
                    plugins.append(pluginJsonConfig);
                } else if (GenerateLocalJsonConfigForCommonPluginMode(pluginYamlNode, pluginJsonConfig)) {
                    plugins.append(pluginJsonConfig);
                }
            }
        } else {
            LOG_ERROR(sLogger,
                      ("GenerateLocalJsonConfigForPluginCategory failed", "plugin category is not sequence.")(
                          "config_name", configName)("plugin_category", pluginCategory));
        }
    }

    if (!plugins.isNull()) {
        if (!(0 == pluginCategory.compare(PLUGIN_CATEGORY_FLUSHERS) && workMode.mHasAccelerateProcessor)) {
            pluginsJsonConfig[pluginCategory] = plugins;
        }
    }
    return true;
}

bool ConfigYamlToJson::GenerateLocalJsonConfigForCommonPluginMode(const YAML::Node& yamlConfig,
                                                                  Json::Value& pluginJsonConfig) {
    pluginJsonConfig["type"] = yamlConfig["Type"].as<std::string>();
    pluginJsonConfig["detail"] = ChangeYamlToJson(yamlConfig);
    pluginJsonConfig["detail"].removeMember("Type");

    return true;
}

bool ConfigYamlToJson::GenerateLocalJsonConfigForFileMode(const YAML::Node& yamlConfig, Json::Value& userJsonConfig) {
    string pluginType = yamlConfig["Type"].as<std::string>();
    unordered_map<string, string>::iterator inputIter = mFilePluginToLogTypeMap.find(pluginType);
    if (inputIter == mFilePluginToLogTypeMap.end()) {
        return false;
    }

    for (YAML::const_iterator it = yamlConfig.begin(); it != yamlConfig.end(); ++it) {
        string key = GetTransforKey(it->first.as<std::string>());
        if (0 == key.size()) {
        } else {
            userJsonConfig[key] = ChangeYamlToJson(it->second);
        }
    }

    return true;
}

bool ConfigYamlToJson::GenerateLocalJsonConfigForSLSFulsher(const YAML::Node& yamlConfig,
                                                            Json::Value& pluginJsonConfig,
                                                            Json::Value& userJsonConfig) {
    string pluginType = yamlConfig["Type"].as<std::string>();
    if (0 != pluginType.compare("flusher_sls")) {
        return false;
    }

    Json::Value pluginDetails;
    for (YAML::const_iterator it = yamlConfig.begin(); it != yamlConfig.end(); ++it) {
        string yamlKey = it->first.as<std::string>();
        string key = GetTransforKey(yamlKey);
        if (0 == key.size()) {
            pluginDetails[yamlKey] = ChangeYamlToJson(it->second);
        } else {
            userJsonConfig[key] = ChangeYamlToJson(it->second);
        }
    }
    pluginDetails.removeMember("Type");
    pluginJsonConfig["detail"] = pluginDetails;
    pluginJsonConfig["type"] = pluginType;

    return true;
}

bool ConfigYamlToJson::FillupDefalutUserJsonConfig(const WorkMode& workMode, Json::Value& userJsonConfig) {
    if (workMode.mIsFileMode) {
        if (!userJsonConfig.isMember("max_depth")) {
            userJsonConfig["max_depth"] = 0;
        }

        // To compatible with the old configuration, introduce placeholder for file mode.
        if (0 == workMode.mInputPluginType.compare(INPUT_FILE_LOG)) {
            if (!userJsonConfig.isMember("regex")) {
                userJsonConfig["regex"][0] = "(.*)";
            }
            if (!userJsonConfig.isMember("keys")) {
                userJsonConfig["keys"][0] = "content";
            }
        }
    }

    return true;
}

} // namespace logtail