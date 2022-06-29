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
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <json/json.h>
#include <yaml-cpp/yaml.h>

using namespace std;
namespace logtail {

struct PluginInfo {
    uint32_t mTimes;
    uint32_t mFirstPos;
};

struct WorkMode {
    bool mIsFileMode;
    string mInputPluginType;

    bool mHasAccelerateProcessor;
    string mAccelerateProcessorPluginType;
    string mLogSplitProcessorPluginType;

    string mLogType;

    void reset() {
        mIsFileMode = false;
        mInputPluginType = "";

        mHasAccelerateProcessor = false;
        mAccelerateProcessorPluginType = "";
        mLogSplitProcessorPluginType = "";

        mLogType = "";
    }
};

class ConfigYamlToJson {
public:
    ConfigYamlToJson();
    static ConfigYamlToJson* GetInstance() {
        static ConfigYamlToJson* ptr = new ConfigYamlToJson();
        return ptr;
    }

    bool
    GenerateLocalJsonConfig(const string configName, const YAML::Node& yamlConfig, Json::Value& userLocalJsonConfig);
    bool CheckPluginConfig(const string configName, const YAML::Node& yamlConfig, WorkMode& workMode);
    Json::Value ChangeYamlToJson(const YAML::Node& root);

private:
    string GetTransforKey(const string yamlKey);
    Json::Value ParseScalar(const YAML::Node& node);

    bool GeneratePluginStatistics(const string pluginCategory,
                                  const YAML::Node& yamlConfig,
                                  unordered_map<string, PluginInfo>& pluginInfos);

    bool GenerateLocalJsonConfigForPluginCategory(const string configName,
                                                  const WorkMode& workMode,
                                                  const string pluginCategory,
                                                  const YAML::Node& yamlConfig,
                                                  Json::Value& pluginsJsonConfig,
                                                  Json::Value& userJsonConfig);
    bool FillupMustMultiLinesSplitProcessor(const WorkMode& workMode, Json::Value& splitProcessor);
    bool GenerateLocalJsonConfigForCommonPluginMode(const YAML::Node& yamlConfig, Json::Value& userJsonConfig);
    bool GenerateLocalJsonConfigForFileMode(const YAML::Node& yamlConfig, Json::Value& userJsonConfig);
    bool GenerateLocalJsonConfigForSLSFulsher(const YAML::Node& yamlConfig,
                                              Json::Value& pluginJsonConfig,
                                              Json::Value& userJsonConfig);
    bool FillupDefalutUserJsonConfig(const WorkMode& workMode, Json::Value& userJsonConfig);

    unordered_map<string, string> mFileConfigMap;
    unordered_map<string, string> mFilePluginToLogTypeMap;
};

} // namespace logtail
