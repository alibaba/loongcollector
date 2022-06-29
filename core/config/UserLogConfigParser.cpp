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

#include "UserLogConfigParser.h"
#include <vector>
#include <boost/filesystem.hpp>
#include "common/FileSystemUtil.h"
#include "common/JsonUtil.h"
#include "common/ExceptionBase.h"
#include "common/LogtailCommonFlags.h"
#include "processor/UnaryFilterOperatorNode.h"
#include "processor/RegexFilterValueNode.h"
#include "processor/BinaryFilterOperatorNode.h"
#include "logger/Logger.h"
#include "Config.h"

namespace logtail {

void UserLogConfigParser::ParseAdvancedConfig(const Json::Value& originalVal, Config& cfg) {
    const std::string ADVANCED_CONFIG_KEY = "advanced";
    if (!originalVal.isMember(ADVANCED_CONFIG_KEY) || !originalVal[ADVANCED_CONFIG_KEY].isObject()) {
        return;
    }
    const auto& advancedVal = originalVal[ADVANCED_CONFIG_KEY];

    auto blacklistException = ParseBlacklist(advancedVal, cfg);
    if (!blacklistException.empty()) {
        throw blacklistException;
    }

    // Boolean force_multiconfig.
    {
        const Json::Value& val = advancedVal["force_multiconfig"];
        if (val.isBool()) {
            cfg.mAdvancedConfig.mForceMultiConfig = val.asBool();
            LOG_INFO(sLogger,
                     ("set force multi config",
                      cfg.mAdvancedConfig.mForceMultiConfig)("project", cfg.mProjectName)("config", cfg.mConfigName));
        }
    }
    // support extract partial fields in DELIMITER_LOG mode
    if (cfg.mLogType == DELIMITER_LOG) {
        if (advancedVal.isMember("extract_partial_fields") && advancedVal["extract_partial_fields"].isBool()) {
            cfg.mAdvancedConfig.mExtractPartialFields = GetBoolValue(advancedVal, "extract_partial_fields");
        }
    }
    // raw log tag
    if (advancedVal.isMember("raw_log_tag") && advancedVal["raw_log_tag"].isString()) {
        std::string rawLogTag = GetStringValue(advancedVal, "raw_log_tag");
        if (!rawLogTag.empty()) {
            cfg.mAdvancedConfig.mRawLogTag = rawLogTag;
        }
    }
    // pass_tags_to_plugin.
    {
        const Json::Value& val = advancedVal["pass_tags_to_plugin"];
        if (val.isBool()) {
            cfg.mAdvancedConfig.mPassTagsToPlugin = val.asBool();
            LOG_INFO(sLogger,
                     ("passing tags to plugin",
                      cfg.mAdvancedConfig.mPassTagsToPlugin)("project", cfg.mProjectName)("logstore", cfg.mCategory));
        }
    }
    // tail_size_kb: this will overwrite tail_limit.
    {
        const Json::Value& val = advancedVal["tail_size_kb"];
        if (val.isInt()) {
            int32_t tailSize = val.asInt();
            cfg.SetTailLimit(tailSize);
            LOG_INFO(sLogger, ("set tail size (KB)", cfg.mTailLimit)("param (KB)", tailSize));
        }
    }
    // batch_send_interval
    {
        const Json::Value& val = advancedVal["batch_send_interval"];
        if (val.isInt()) {
            cfg.mAdvancedConfig.mBatchSendInterval = val.asInt();
            LOG_INFO(sLogger, ("set batch send interval", cfg.mAdvancedConfig.mBatchSendInterval));
        }
    }
    // log filter: AND/OR/NOT/REGEX.
    {
        const Json::Value& val = advancedVal["filter_expression"];
        if (!val.isNull()) {
            BaseFilterNodePtr root = ParseExpressionFromJSON(val);
            if (!root) {
                throw ExceptionBase("invalid filter expression: " + val.toStyledString());
            }
            cfg.mAdvancedConfig.mFilterExpressionRoot.swap(root);
            LOG_INFO(sLogger, ("parse filter expression", val.toStyledString()));
        }
    }
    // max_rotate_queue_size
    {
        const Json::Value& val = advancedVal["max_rotate_queue_size"];
        if (val.isInt()) {
            cfg.mAdvancedConfig.mMaxRotateQueueSize = val.asInt();
            LOG_INFO(sLogger, ("set max rotate queue size", cfg.mAdvancedConfig.mMaxRotateQueueSize));
        }
    }
    // close_unused_reader_interval
    {
        const Json::Value& val = advancedVal["close_unused_reader_interval"];
        if (val.isInt()) {
            cfg.mAdvancedConfig.mCloseUnusedReaderInterval = val.asInt();
            LOG_INFO(sLogger, ("set close unused reader interval", cfg.mAdvancedConfig.mCloseUnusedReaderInterval));
        }
    }
    // exactly once
    {
        const auto& val = advancedVal["exactly_once_concurrency"];
        if (val.isUInt()) {
            auto concurrency = val.asUInt();
            if (concurrency > Config::kExactlyOnceMaxConcurrency) {
                throw ExceptionBase(std::string("invalid exactly once concurrency, range: [0, ")
                                    + std::to_string(Config::kExactlyOnceMaxConcurrency) + "]");
            }
            cfg.mAdvancedConfig.mExactlyOnceConcurrency = concurrency;
            LOG_INFO(sLogger, ("set exactly once concurrency", concurrency));
        }
        if (cfg.mAdvancedConfig.mExactlyOnceConcurrency > 0 && cfg.mPluginProcessFlag) {
            throw ExceptionBase("exactly once can not be enabled while plugin.processors exist");
        }
        if (cfg.mAdvancedConfig.mExactlyOnceConcurrency > 0 && cfg.mMergeType != MERGE_BY_TOPIC) {
            throw ExceptionBase("exactly once must use MERGE_BY_TOPIC");
        }
    }
    // inode tag and offset in each log.
    {
        const auto& val = advancedVal["enable_log_position_meta"];
        if (val.isBool()) {
            cfg.mAdvancedConfig.mEnableLogPositionMeta = val.asBool();
        }
    }
    // specified_year: fill year in time.
    {
        const auto& val = advancedVal["specified_year"];
        if (val.isUInt()) {
            cfg.mAdvancedConfig.mSpecifiedYear = static_cast<int32_t>(val.asUInt());
        }
    }
    // precise_timestamp
    {
        if (advancedVal.isMember("enable_precise_timestamp") && advancedVal["enable_precise_timestamp"].isBool()) {
            cfg.mAdvancedConfig.mEnablePreciseTimestamp = GetBoolValue(advancedVal, "enable_precise_timestamp");
        }
        if (cfg.mAdvancedConfig.mEnablePreciseTimestamp) {
            if (advancedVal.isMember("precise_timestamp_key") && advancedVal["precise_timestamp_key"].isString()) {
                cfg.mAdvancedConfig.mPreciseTimestampKey = GetStringValue(advancedVal, "precise_timestamp_key");
            }
            if (cfg.mAdvancedConfig.mPreciseTimestampKey.empty()) {
                cfg.mAdvancedConfig.mPreciseTimestampKey = PRECISE_TIMESTAMP_DEFAULT_KEY;
            }

            if (advancedVal.isMember("precise_timestamp_unit") && advancedVal["precise_timestamp_unit"].isString()) {
                std::string key = GetStringValue(advancedVal, "precise_timestamp_unit");
                if (0 == key.compare("ms")) {
                    cfg.mAdvancedConfig.mPreciseTimestampUnit = TimeStampUnit::MILLISECOND;
                } else if (0 == key.compare("us")) {
                    cfg.mAdvancedConfig.mPreciseTimestampUnit = TimeStampUnit::MICROSECOND;
                } else if (0 == key.compare("ns")) {
                    cfg.mAdvancedConfig.mPreciseTimestampUnit = TimeStampUnit::NANOSECOND;
                } else {
                    cfg.mAdvancedConfig.mPreciseTimestampUnit = TimeStampUnit::MILLISECOND;
                }
            } else {
                cfg.mAdvancedConfig.mPreciseTimestampUnit = TimeStampUnit::MILLISECOND;
            }
        }
    }
    // search_checkpoint_dir_depth.
    {
        const auto& val = advancedVal["search_checkpoint_dir_depth"];
        if (val.isUInt()) {
            cfg.mAdvancedConfig.mSearchCheckpointDirDepth = static_cast<uint16_t>(val.asUInt());
        }
    }
    // enable_root_path_collection.
    if (!BOOL_FLAG(enable_root_path_collection)) {
        const auto& val = advancedVal["enable_root_path_collection"];
        if (val.isBool() && val.asBool()) {
            BOOL_FLAG(enable_root_path_collection) = true;
            LOG_INFO(sLogger, ("message", "enable root path collection"));
        }
    }
}

// Configurations:
// blacklist: {
//   Array<String> dir_blacklist;
//   Array<String> filename_blacklist;
//   Array<String> filepath_blacklist;
// }
std::string UserLogConfigParser::ParseBlacklist(const Json::Value& advancedVal, Config& cfg) {
    if (!(advancedVal.isMember("blacklist") && advancedVal["blacklist"].isObject())) {
        return "";
    }

    std::string exceptionStr;
    const auto& val = advancedVal["blacklist"];
    if (val.isMember("dir_blacklist") && val["dir_blacklist"].isArray()) {
        for (auto const& iter : val["dir_blacklist"]) {
            if (!iter.isString()) {
                exceptionStr += "dir_blacklist item must be string\n";
                continue;
            }
            auto const path = iter.asString();
            if (!boost::filesystem::path(path).is_absolute()) {
                exceptionStr += "dir_blacklist item must be absolute path: " + path + "\n";
                continue;
            }
            bool isMultipleLevelWildcard = path.find("**") != std::string::npos;
            if (isMultipleLevelWildcard) {
                cfg.mMLWildcardDirPathBlacklist.push_back(path);
                LOG_INFO(sLogger,
                         ("add dir_blacklist, multiple level", path)("project", cfg.mProjectName)("config",
                                                                                                  cfg.mConfigName));
                continue;
            }
            bool isWildcardPath = path.find("*") != std::string::npos || path.find("?") != std::string::npos;
            if (isWildcardPath) {
                cfg.mWildcardDirPathBlacklist.push_back(path);
            } else {
                cfg.mDirPathBlacklist.push_back(path);
            }
            LOG_INFO(sLogger,
                     ("add dir_blacklist",
                      path)("is wildcard", isWildcardPath)("project", cfg.mProjectName)("config", cfg.mConfigName));
        }
    }

    if (val.isMember("filename_blacklist") && val["filename_blacklist"].isArray()) {
        for (auto const& iter : val["filename_blacklist"]) {
            if (!iter.isString()) {
                exceptionStr += "filename_blacklist item must be string\n";
                continue;
            }
            auto const name = iter.asString();
            if (std::string::npos != name.find(PATH_SEPARATOR)) {
                exceptionStr += "filename_blacklist item can not contain path separator, "
                                "file name: "
                    + name + "\n";
                continue;
            }
            cfg.mFileNameBlacklist.push_back(name);
            LOG_INFO(sLogger, ("add filename_blacklist", name)("project", cfg.mProjectName)("config", cfg.mConfigName));
        }
    }

    if (val.isMember("filepath_blacklist") && val["filepath_blacklist"].isArray()) {
        for (auto const& iter : val["filepath_blacklist"]) {
            if (!iter.isString()) {
                exceptionStr += "filepath_blacklist item must be string\n";
                continue;
            }
            auto const path = iter.asString();
            if (!boost::filesystem::path(path).is_absolute()) {
                exceptionStr += "filepath_blacklist item must be absolute path, file path: " + path + "\n";
                continue;
            }
            bool isMultipleLevelWildcard = path.find("**") != std::string::npos;
            if (isMultipleLevelWildcard) {
                cfg.mMLFilePathBlacklist.push_back(path);
            } else {
                cfg.mFilePathBlacklist.push_back(path);
            }
            LOG_INFO(sLogger,
                     ("add filepath_blacklist", path)("wildcard", isMultipleLevelWildcard)("project", cfg.mProjectName)(
                         "config", cfg.mConfigName));
        }
    }

    if (!cfg.mDirPathBlacklist.empty() || !cfg.mWildcardDirPathBlacklist.empty()
        || !cfg.mMLWildcardDirPathBlacklist.empty() || !cfg.mMLFilePathBlacklist.empty()
        || !cfg.mFileNameBlacklist.empty() || !cfg.mFilePathBlacklist.empty()) {
        cfg.mHasBlacklist = true;
        LOG_INFO(sLogger, ("set has blacklist", cfg.mProjectName + "#" + cfg.mConfigName));
    }

    return exceptionStr;
}

BaseFilterNodePtr UserLogConfigParser::ParseExpressionFromJSON(const Json::Value& value) {
    BaseFilterNodePtr node;
    if (!value.isObject()) {
        return node;
    }

    if (value["operator"].isString() && value["operands"].isArray()) {
        std::string op = ToLowerCaseString(value["operator"].asString());
        FilterOperator filterOperator;
        // check operator
        if (!GetOperatorType(op, filterOperator)) {
            return node;
        }

        // check operands
        // if "op" element occurs, "operands" element must exist its type must be array, otherwise we consider it as invalid json
        const Json::Value& operandsValue = value["operands"];
        if (filterOperator == NOT_OPERATOR && operandsValue.size() == 1) {
            BaseFilterNodePtr childNode = ParseExpressionFromJSON(operandsValue[0]);
            if (childNode) {
                node.reset(new UnaryFilterOperatorNode(filterOperator, childNode));
            }
        } else if ((filterOperator == AND_OPERATOR || filterOperator == OR_OPERATOR) && operandsValue.size() == 2) {
            BaseFilterNodePtr leftNode = ParseExpressionFromJSON(operandsValue[0]);
            BaseFilterNodePtr rightNode = ParseExpressionFromJSON(operandsValue[1]);
            if (leftNode && rightNode) {
                node.reset(new BinaryFilterOperatorNode(filterOperator, leftNode, rightNode));
            }
        }
    } else if ((value["key"].isString() && value["exp"].isString()) || !value["type"].isString()) {
        std::string key = value["key"].asString();
        std::string exp = value["exp"].asString();
        std::string type = ToLowerCaseString(value["type"].asString());

        FilterNodeFunctionType func;
        if (!GetNodeFuncType(type, func)) {
            return node;
        }
        if (func == REGEX_FUNCTION) {
            node.reset(new RegexFilterValueNode(key, exp));
        }
    }
    return node;
}

bool UserLogConfigParser::GetOperatorType(const std::string& type, FilterOperator& op) {
    if (type == "not") {
        op = NOT_OPERATOR;
    } else if (type == "and") {
        op = AND_OPERATOR;
    } else if (type == "or") {
        op = OR_OPERATOR;
    } else {
        LOG_ERROR(sLogger, ("invalid operator", type));
        return false;
    }
    return true;
}

bool UserLogConfigParser::GetNodeFuncType(const std::string& type, FilterNodeFunctionType& func) {
    if (type == "regex") {
        func = REGEX_FUNCTION;
    } else {
        LOG_ERROR(sLogger, ("invalid func type", type));
        return false;
    }
    return true;
}

} // namespace logtail
