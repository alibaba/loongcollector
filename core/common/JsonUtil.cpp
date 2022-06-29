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

#include "JsonUtil.h"
#include "logger/Logger.h"
#include "ExceptionBase.h"
#include "StringTools.h"

namespace logtail {

bool IsValidJson(const char* buffer, int32_t size) {
    int32_t idx = 0;
    while (idx < size) {
        if (buffer[idx] == ' ' || buffer[idx] == '\n' || buffer[idx] == '\t')
            idx++;
        else
            break;
    }
    if (idx == size || (buffer[idx] != '{' && buffer[idx] != '[')) {
        return false;
    }

    int32_t braceCount = 0;
    bool inQuote = false;
    for (; idx < size; ++idx) {
        switch (buffer[idx]) {
            case '{':
                if (!inQuote)
                    braceCount++;
                break;
            case '}':
                if (!inQuote)
                    braceCount--;
                if (braceCount < 0) {
                    return false;
                }
                break;
            case '"':
                inQuote = !inQuote;
                break;
            case '\\':
                ++idx; //skip next char after escape char
                break;
            default:
                break;
        }
    }
    return braceCount == 0;
}

void CheckNameExist(const Json::Value& value, const std::string& name) {
    if (value.isMember(name) == false) {
        throw ExceptionBase(std::string("The key '") + name + "' not exist");
    }
}

bool GetBoolValue(const Json::Value& value, const std::string& name) {
    CheckNameExist(value, name);
    if (value[name].isBool() == false) {
        throw ExceptionBase(std::string("The key '") + name + "' not valid bool value");
    }
    return value[name].asBool();
}

bool GetBoolValue(const Json::Value& value, const std::string& name, const bool defValue) {
    if (value.isMember(name) == false) {
        return defValue;
    }
    return GetBoolValue(value, name);
}

std::string GetStringValue(const Json::Value& value, const std::string& name) {
    CheckNameExist(value, name);
    if (value[name].isString() == false) {
        throw ExceptionBase(std::string("The key '") + name + "' not valid string value");
    }
    return value[name].asString();
}

std::string GetStringValue(const Json::Value& value, const std::string& name, const std::string& defValue) {
    if (value.isMember(name) == false) {
        return defValue;
    }
    return GetStringValue(value, name);
}

int32_t GetIntValue(const Json::Value& value, const std::string& name) {
    CheckNameExist(value, name);
    if (value[name].isInt() == false) {
        throw ExceptionBase(std::string("The key '") + name + "' not valid int value");
    }
    return value[name].asInt();
}

int32_t GetIntValue(const Json::Value& value, const std::string& name, const int32_t defValue) {
    if (value.isMember(name) == false) {
        return defValue;
    }
    return GetIntValue(value, name);
}

int64_t GetInt64Value(const Json::Value& value, const std::string& name) {
    CheckNameExist(value, name);
    if (value[name].isInt() == false) {
        throw ExceptionBase(std::string("The key '") + name + "' not valid int value");
    }
    return value[name].asInt64();
}

int64_t GetInt64Value(const Json::Value& value, const std::string& name, const int64_t defValue) {
    if (value.isMember(name) == false) {
        return defValue;
    }
    return GetInt64Value(value, name);
}

namespace {

    // @return true if succeed to load value from env.
    template <typename T>
    bool LoadEnvValueIfExisting(const char* envKey, T& cfgValue) {
        try {
            const char* value = getenv(envKey);
            if (value != NULL) {
                T val = StringTo<T>(value);
                cfgValue = val;
                APSARA_LOG_INFO(sLogger, ("load config from env", envKey)("value", val));
                return true;
            }
        } catch (const std::exception& e) {
            APSARA_LOG_WARNING(sLogger, ("load config from env error", envKey)("error", e.what()));
        }
        return false;
    }

} // namespace

#define LOAD_PARAMETER(outVal, confJSON, name, envName, testJSONFunc, convertJSONFunc) \
    bool loaded = false; \
    if (name != NULL && LoadEnvValueIfExisting(name, outVal)) { \
        loaded = true; \
        APSARA_LOG_INFO(sLogger, ("load parameter from env", name)("value", outVal)); \
    } \
    if (envName != NULL && LoadEnvValueIfExisting(envName, value)) { \
        loaded = true; \
        APSARA_LOG_INFO(sLogger, ("load parameter from env", envName)("value", outVal)); \
    } \
    if (name != NULL && (confJSON.isMember(name) && confJSON[name].testJSONFunc())) { \
        outVal = confJSON[name].convertJSONFunc(); \
        loaded = true; \
        APSARA_LOG_INFO(sLogger, ("load parameter from JSON", name)("value", outVal)); \
    } \
    return loaded;

bool LoadInt32Parameter(int32_t& value, const Json::Value& confJSON, const char* name, const char* envName) {
    LOAD_PARAMETER(value, confJSON, name, envName, isInt, asInt)
}

bool LoadStringParameter(std::string& value, const Json::Value& confJSON, const char* name, const char* envName) {
    LOAD_PARAMETER(value, confJSON, name, envName, isString, asString)
}

bool LoadBooleanParameter(bool& value, const Json::Value& confJSON, const char* name, const char* envName) {
    LOAD_PARAMETER(value, confJSON, name, envName, isBool, asBool)
}

} // namespace logtail
