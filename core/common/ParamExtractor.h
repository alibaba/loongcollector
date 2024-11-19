/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <json/json.h>

#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "common/StringTools.h"
#include "logger/Logger.h"
#include "monitor/AlarmManager.h"

#define PARAM_ERROR_RETURN(logger, alarm, msg, module, config, project, logstore, region) \
    if (module.empty()) { \
        LOG_ERROR(logger, ("failed to parse config", msg)("config", config)); \
        alarm.SendAlarm( \
            CATEGORY_CONFIG_ALARM, std::string(msg) + ": abort, config: " + config, project, logstore, region); \
    } else { \
        LOG_ERROR(logger, ("failed to parse config", msg)("module", module)("config", config)); \
        alarm.SendAlarm(CATEGORY_CONFIG_ALARM, \
                        std::string(msg) + ": abort, module: " + module + ", config: " + config, \
                        project, \
                        logstore, \
                        region); \
    } \
    return false
#define PARAM_WARNING_IGNORE(logger, alarm, msg, module, config, project, logstore, region) \
    if (module.empty()) { \
        LOG_WARNING(logger, \
                    ("problem encountered in config parsing", msg)("action", "ignore param")("config", config)); \
        alarm.SendAlarm( \
            CATEGORY_CONFIG_ALARM, std::string(msg) + ": ignore param, config: " + config, project, logstore, region); \
    } else { \
        LOG_WARNING(logger, \
                    ("problem encountered in config parsing", \
                     msg)("action", "ignore param")("module", module)("config", config)); \
        alarm.SendAlarm(CATEGORY_CONFIG_ALARM, \
                        std::string(msg) + ": ignore param, module: " + module + ", config: " + config, \
                        project, \
                        logstore, \
                        region); \
    }
#define PARAM_WARNING_DEFAULT(logger, alarm, msg, val, module, config, project, logstore, region) \
    if (module.empty()) { \
        LOG_WARNING(logger, \
                    ("problem encountered in config parsing", \
                     msg)("action", "use default value instead")("default value", ToString(val))("config", config)); \
        alarm.SendAlarm(CATEGORY_CONFIG_ALARM, \
                        std::string(msg) + ": use default value instead, default value: " + ToString(val) \
                            + ", config: " + config, \
                        project, \
                        logstore, \
                        region); \
    } else { \
        LOG_WARNING(logger, \
                    ("problem encountered in config parsing", msg)("action", "use default value instead")( \
                        "default value", ToString(val))("module", module)("config", config)); \
        alarm.SendAlarm(CATEGORY_CONFIG_ALARM, \
                        std::string(msg) + ": use default value instead, default value: " + ToString(val) \
                            + ", module: " + module + ", config: " + config, \
                        project, \
                        logstore, \
                        region); \
    }

#define TASK_PARAM_ERROR_RETURN(logger, alarm, msg, module, config) \
    if (module.empty()) { \
        LOG_ERROR(logger, ("failed to parse config", msg)("config", config)); \
        alarm.SendAlarm(CATEGORY_CONFIG_ALARM, std::string(msg) + ": abort, config: " + config); \
    } else { \
        LOG_ERROR(logger, ("failed to parse config", msg)("module", module)("config", config)); \
        alarm.SendAlarm(CATEGORY_CONFIG_ALARM, \
                        std::string(msg) + ": abort, module: " + module + ", config: " + config); \
    } \
    return false
#define TASK_PARAM_WARNING_IGNORE(logger, alarm, msg, module, config) \
    if (module.empty()) { \
        LOG_WARNING(logger, \
                    ("problem encountered in config parsing", msg)("action", "ignore param")("config", config)); \
        alarm.SendAlarm(CATEGORY_CONFIG_ALARM, std::string(msg) + ": ignore param, config: " + config); \
    } else { \
        LOG_WARNING(logger, \
                    ("problem encountered in config parsing", \
                     msg)("action", "ignore param")("module", module)("config", config)); \
        alarm.SendAlarm(CATEGORY_CONFIG_ALARM, \
                        std::string(msg) + ": ignore param, module: " + module + ", config: " + config); \
    }
#define TASK_PARAM_WARNING_DEFAULT(logger, alarm, msg, val, module, config) \
    if (module.empty()) { \
        LOG_WARNING(logger, \
                    ("problem encountered in config parsing", \
                     msg)("action", "use default value instead")("default value", ToString(val))("config", config)); \
        alarm.SendAlarm(CATEGORY_CONFIG_ALARM, \
                        std::string(msg) + ": use default value instead, default value: " + ToString(val) \
                            + ", config: " + config); \
    } else { \
        LOG_WARNING(logger, \
                    ("problem encountered in config parsing", msg)("action", "use default value instead")( \
                        "default value", ToString(val))("module", module)("config", config)); \
        alarm.SendAlarm(CATEGORY_CONFIG_ALARM, \
                        std::string(msg) + ": use default value instead, default value: " + ToString(val) \
                            + ", module: " + module + ", config: " + config); \
    }

namespace logtail {

const std::string noModule = "";

std::string ExtractCurrentKey(const std::string& key);

bool GetOptionalBoolParam(const Json::Value& config, const std::string& key, bool& param, std::string& errorMsg);

bool GetOptionalIntParam(const Json::Value& config, const std::string& key, int32_t& param, std::string& errorMsg);

bool GetOptionalUIntParam(const Json::Value& config, const std::string& key, uint32_t& param, std::string& errorMsg);

bool GetOptionalStringParam(const Json::Value& config,
                            const std::string& key,
                            std::string& param,
                            std::string& errorMsg);

bool GetOptionalDoubleParam(const Json::Value& config, const std::string& key, double& param, std::string& errorMsg);

template <class T>
bool GetOptionalListParam(const Json::Value& config,
                          const std::string& key,
                          std::vector<T>& param,
                          std::string& errorMsg) {
    errorMsg.clear();
    std::string currentKey = ExtractCurrentKey(key);
    const Json::Value* itr = config.find(currentKey.c_str(), currentKey.c_str() + currentKey.length());
    if (itr) {
        if (!itr->isArray()) {
            errorMsg = "param " + key + " is not of type list";
            return false;
        }
        for (auto it = itr->begin(); it != itr->end(); ++it) {
            if constexpr (std::is_same_v<T, bool>) {
                if (!it->isBool()) {
                    errorMsg = "element in list param " + key + " is not of type bool";
                    return false;
                }
                param.emplace_back(it->asBool());
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                if (!it->isUInt()) {
                    errorMsg = "element in list param " + key + " is not of type uint";
                    return false;
                }
                param.emplace_back(it->asUInt());
            } else if constexpr (std::is_same_v<T, int32_t>) {
                if (!it->isInt()) {
                    errorMsg = "element in list param " + key + " is not of type int";
                    return false;
                }
                param.emplace_back(it->asInt());
            } else if constexpr (std::is_same_v<T, std::string>) {
                if (!it->isString()) {
                    errorMsg = "element in list param " + key + " is not of type string";
                    return false;
                }
                param.emplace_back(it->asString());
            } else {
                errorMsg = "element in list param " + key + " is not supported";
                return false;
            }
        }
    }
    return true;
}

template <class T>
bool GetOptionalListFilterParam(const Json::Value& config,
                                const std::string& key,
                                std::vector<T>& param,
                                std::string& errorMsg) {
    errorMsg.clear();
    std::string currentKey = ExtractCurrentKey(key);
    const Json::Value* itr = config.find(currentKey.c_str(), currentKey.c_str() + currentKey.length());
    if (itr) {
        if (!itr->isArray()) {
            errorMsg = "param " + key + " is not of type list";
            return false;
        }
        for (auto it = itr->begin(); it != itr->end(); ++it) {
            if constexpr (std::is_same_v<T, bool>) {
                if (!it->isBool()) {
                    errorMsg = "element in list param " + key + " is not of type bool";
                    param.clear();
                    return false;
                }
                param.emplace_back(it->asBool());
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                if (!it->isUInt()) {
                    errorMsg = "element in list param " + key + " is not of type uint";
                    param.clear();
                    return false;
                }
                param.emplace_back(it->asUInt());
            } else if constexpr (std::is_same_v<T, int32_t>) {
                if (!it->isInt()) {
                    errorMsg = "element in list param " + key + " is not of type int";
                    param.clear();
                    return false;
                }
                param.emplace_back(it->asInt());
            } else if constexpr (std::is_same_v<T, std::string>) {
                if (!it->isString()) {
                    errorMsg = "element in list param " + key + " is not of type string";
                    param.clear();
                    return false;
                }
                param.emplace_back(it->asString());
            } else {
                errorMsg = "element in list param " + key + " is not supported";
                param.clear();
                return false;
            }
        }
    }
    return true;
}

template <class T>
bool GetOptionalMapParam(const Json::Value& config,
                         const std::string& key,
                         std::unordered_map<std::string, T>& param,
                         std::string& errorMsg) {
    errorMsg.clear();
    std::string currentKey = ExtractCurrentKey(key);
    const Json::Value* itr = config.find(currentKey.c_str(), currentKey.c_str() + currentKey.length());
    if (itr) {
        if (!itr->isObject()) {
            errorMsg = "param " + key + " is not of type map";
            return false;
        }
        for (auto it = itr->begin(); it != itr->end(); ++it) {
            if constexpr (std::is_same_v<T, bool>) {
                if (!it->isBool()) {
                    errorMsg = "value in map param " + key + " is not of type bool";
                    return false;
                }
                param[it.name()] = it->asBool();
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                if (!it->isUInt()) {
                    errorMsg = "value in map param " + key + " is not of type uint";
                    return false;
                }
                param[it.name()] = it->asUInt();
            } else if constexpr (std::is_same_v<T, int32_t>) {
                if (!it->isInt()) {
                    errorMsg = "value in map param " + key + " is not of type int";
                    return false;
                }
                param[it.name()] = it->asInt();
            } else if constexpr (std::is_same_v<T, std::string>) {
                if (!it->isString()) {
                    errorMsg = "value in map param " + key + " is not of type string";
                    return false;
                }
                param[it.name()] = it->asString();
            } else {
                errorMsg = "value in map param " + key + " is not supported";
                return false;
            }
        }
    }
    return true;
}

bool GetMandatoryBoolParam(const Json::Value& config, const std::string& key, bool& param, std::string& errorMsg);

bool GetMandatoryIntParam(const Json::Value& config, const std::string& key, int32_t& param, std::string& errorMsg);

bool GetMandatoryUIntParam(const Json::Value& config, const std::string& key, uint32_t& param, std::string& errorMsg);

bool GetMandatoryStringParam(const Json::Value& config,
                             const std::string& key,
                             std::string& param,
                             std::string& errorMsg);

bool GetMandatoryDoubleParam(const Json::Value& config, const std::string& key, double& param, std::string& errorMsg);

template <class T>
bool GetMandatoryListParam(const Json::Value& config,
                           const std::string& key,
                           std::vector<T>& param,
                           std::string& errorMsg) {
    errorMsg.clear();
    if (!config.isMember(ExtractCurrentKey(key))) {
        errorMsg = "madatory param " + key + " is missing";
        return false;
    }
    if (!GetOptionalListParam<T>(config, key, param, errorMsg)) {
        return false;
    }
    if (param.empty()) {
        errorMsg = "madatory list param " + key + " is empty";
        return false;
    }
    return true;
}

bool IsRegexValid(const std::string& regStr);

bool IsValidList(const Json::Value& config, const std::string& key, std::string& errorMsg);

bool IsValidMap(const Json::Value& config, const std::string& key, std::string& errorMsg);

} // namespace logtail
