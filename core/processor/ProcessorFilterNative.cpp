/*
 * Copyright 2023 iLogtail Authors
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

#include "processor/ProcessorFilterNative.h"
#include "common/Constants.h"
#include "models/LogEvent.h"
#include "plugin/instance/ProcessorInstance.h"
#include "config_manager/ConfigManager.h"
#include "monitor/MetricConstants.h"
#include <vector>

namespace logtail {
const std::string ProcessorFilterNative::sName = "processor_filter_native";

ProcessorFilterNative::~ProcessorFilterNative() {
    for (auto& mFilter : mFilters) {
        delete mFilter.second;
    }
    mFilters.clear();
}

bool ProcessorFilterNative::Init(const ComponentConfig& componentConfig) {
    const PipelineConfig& config = componentConfig.GetConfig();

    if (config.mAdvancedConfig.mFilterExpressionRoot.get() != nullptr) {
        mFilterExpressionRoot = config.mAdvancedConfig.mFilterExpressionRoot;
        mFilterMode = EXPRESSION_MODE;
    } else if (config.mFilterRule) {
        mFilterRule = config.mFilterRule;
        mFilterMode = RULE_MODE;
    } else if (LoadOldGlobalConfig(config)) {
        mFilterMode = GLOBAL_MODE;
    } else {
        mFilterMode = BYPASS_MODE;
    }

    mDiscardNoneUtf8 = config.mDiscardNoneUtf8;

    mProcFilterInSizeBytes = GetMetricsRecordRef().CreateCounter(METRIC_PROC_FILTER_IN_SIZE_BYTES);
    mProcFilterOutSizeBytes = GetMetricsRecordRef().CreateCounter(METRIC_PROC_FILTER_OUT_SIZE_BYTES);
    mProcFilterErrorTotal = GetMetricsRecordRef().CreateCounter(METRIC_PROC_FILTER_ERROR_TOTAL);
    mProcFilterRecordsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PROC_FILTER_RECORDS_TOTAL);

    return true;
}

bool ProcessorFilterNative::LoadOldGlobalConfig(const PipelineConfig& config) {
    // old InitFilter
    Json::Value jsonRoot; // will contains the root value after parsing.
    ParseConfResult userLogRes = ParseConfig(STRING_FLAG(user_log_config), jsonRoot);
    if (userLogRes != CONFIG_OK) {
        if (userLogRes == CONFIG_NOT_EXIST)
            LOG_DEBUG(GetContext().GetLogger(), (config.mConfigName, "not found, uninitialized Filter"));
        if (userLogRes == CONFIG_INVALID_FORMAT)
            LOG_ERROR(GetContext().GetLogger(),
                      ("load user config for filter fail, file content is not valid json", config.mConfigName));
        return false;
    }
    if (!jsonRoot.isMember("filters")) {
        return false;
    }
    const Json::Value& filters = jsonRoot["filters"];
    uint32_t filterSize = filters.size();
    if (!filterSize) {
        return false;
    }
    try {
        for (uint32_t index = 0; index < filterSize; index++) {
            const Json::Value& item = filters[index];
            std::string projectName = item["project_name"].asString();
            std::string category = item["category"].asString();
            const Json::Value& keys = item["keys"];
            const Json::Value& regs = item["regs"];
            if (keys.size() != regs.size()) {
                LOG_ERROR(GetContext().GetLogger(),
                          ("invalid filter config", "ignore it")("project", projectName)("logstore", category));
                return false;
            }
            // default filter relation is AND
            LogFilterRule* filterRule = new LogFilterRule();
            for (uint32_t i = 0; i < keys.size(); i++) {
                filterRule->FilterKeys.push_back(keys[i].asString());
                filterRule->FilterRegs.push_back(boost::regex(regs[i].asString()));
            }
            mFilters[projectName + "_" + category] = filterRule;
        }
    } catch (...) {
        LOG_ERROR(GetContext().GetLogger(), ("Can't parse the filter config", ""));
        return false;
    }
    return true;
}

void ProcessorFilterNative::Process(PipelineEventGroup& logGroup) {
    if (logGroup.GetEvents().empty()) {
        return;
    }

    EventsContainer& events = logGroup.MutableEvents();

    for (auto it = events.begin(); it != events.end();) {
        if (ProcessEvent(*it)) {
            ++it;
        } else {
            it = events.erase(it);
            mProcFilterRecordsTotal->Add(1);
        }
    }
}

bool ProcessorFilterNative::ProcessEvent(PipelineEventPtr& e) {
    if (!IsSupportedEvent(e)) {
        return true;
    }

    auto& sourceEvent = e.Cast<LogEvent>();
    bool res = true;

    if (mFilterMode == EXPRESSION_MODE) {
        res = FilterExpressionRoot(sourceEvent, mFilterExpressionRoot);
    } else if (mFilterMode == RULE_MODE) {
        res = FilterFilterRule(sourceEvent, mFilterRule.get());
    } else if (mFilterMode == GLOBAL_MODE) {
        res = FilterGlobal(sourceEvent);
    }
    if (res && mDiscardNoneUtf8) {
        LogContents& contents = sourceEvent.MutableContents();
        std::vector<std::pair<StringView,StringView> > newContents;
        for (auto content = contents.begin(); content != contents.end();) {
            if (CheckNoneUtf8(content->second)) {
                auto value = content->second.to_string();
                FilterNoneUtf8(value);
                StringBuffer valueBuffer = sourceEvent.GetSourceBuffer()->CopyString(value);
                content->second = StringView(valueBuffer.data, valueBuffer.size);
            }
            if (CheckNoneUtf8(content->first)) {
                // key
                auto key = content->first.to_string();
                FilterNoneUtf8(key);
                StringBuffer keyBuffer = sourceEvent.GetSourceBuffer()->CopyString(key);

                newContents.emplace_back(StringView(keyBuffer.data, keyBuffer.size), content->second);
                content = contents.erase(content);
            } else {
                content++;
            }
        }
        for (auto& newContent : newContents) {
            sourceEvent.SetContentNoCopy(newContent.first, newContent.second);
        }
    }

    return res;
}

bool ProcessorFilterNative::IsSupportedEvent(const PipelineEventPtr& e) const {
    return e.Is<LogEvent>();
}

bool ProcessorFilterNative::FilterExpressionRoot(LogEvent& sourceEvent, const BaseFilterNodePtr& node) {
    const LogContents& contents = sourceEvent.GetContents();
    if (contents.empty()) {
        return false;
    }

    // null node, all logs are passed
    if (node.get() == nullptr) {
        return true;
    }

    try {
        return node->Match(contents, GetContext());
    } catch (...) {
        mProcFilterErrorTotal->Add(1);
        LOG_ERROR(GetContext().GetLogger(), ("filter error ", ""));
        return false;
    }
}

bool ProcessorFilterNative::FilterFilterRule(LogEvent& sourceEvent, const LogFilterRule* filterRule) {
    const LogContents& contents = sourceEvent.GetContents();
    if (contents.empty()) {
        return false;
    }

    // null filterRule, all logs are passed
    if (filterRule == NULL) {
        return true;
    }

    try {
        return IsMatched(contents, *filterRule);
    } catch (...) {
        mProcFilterErrorTotal->Add(1);
        LOG_ERROR(GetContext().GetLogger(), ("filter error ", ""));
        return false;
    }
}

bool ProcessorFilterNative::FilterGlobal(LogEvent& sourceEvent) {
    const LogContents& contents = sourceEvent.GetContents();
    if (contents.empty()) {
        return false;
    }

    std::string key(GetContext().GetProjectName() + "_" + GetContext().GetLogstoreName());
    std::unordered_map<std::string, LogFilterRule*>::iterator it = mFilters.find(key);
    if (it == mFilters.end()) {
        return true;
    }

    const LogFilterRule& rule = *(it->second);
    try {
        return IsMatched(contents, rule);
    } catch (...) {
        mProcFilterErrorTotal->Add(1);
        LOG_ERROR(GetContext().GetLogger(), ("filter error ", ""));
        return false;
    }
}

bool ProcessorFilterNative::IsMatched(const LogContents& contents, const LogFilterRule& rule) {
    const std::vector<std::string>& keys = rule.FilterKeys;
    const std::vector<boost::regex> regs = rule.FilterRegs;
    std::string exception;
    for (uint32_t i = 0; i < keys.size(); i++) {
        const auto& content = contents.find(keys[i]);
        if (content == contents.end()) {
            return false;
        }
        if (!BoostRegexMatch(content->second.data(), content->second.size(), regs[i], exception)) {
            if (!exception.empty()) {
                LOG_ERROR(GetContext().GetLogger(), ("regex_match in Filter fail", exception));
                if (GetContext().GetAlarm().IsLowLevelAlarmValid()) {
                    GetContext().GetAlarm().SendAlarm(REGEX_MATCH_ALARM,
                                                      "regex_match in Filter fail:" + exception,
                                                      GetContext().GetProjectName(),
                                                      GetContext().GetLogstoreName(),
                                                      GetContext().GetRegion());
                }
            }
            return false;
        }
    }
    return true;
}

static const char UTF8_BYTE_PREFIX = 0x80;
static const char UTF8_BYTE_MASK = 0xc0;


bool ProcessorFilterNative::CheckNoneUtf8(const StringView& strSrc) {
    return noneUtf8(const_cast<StringView&>(strSrc), false);
}

void ProcessorFilterNative::FilterNoneUtf8(std::string& strSrc) {
    StringView strView(strSrc.data(), strSrc.size());
    noneUtf8(strView, true);
}

bool ProcessorFilterNative::noneUtf8(StringView& str, bool modify) {
#define FILL_BLUNK_AND_CONTINUE_IF_TRUE(stat) \
    if (stat) { \
        if (!modify) { \
            return true; \
        } \
        const_cast<char&>(*iter) = ' '; \
        ++iter; \
        continue; \
    };

    auto iter = str.begin();
    while (iter != str.end()) {
        uint16_t unicode = 0;
        char c;
        if ((*iter & 0x80) == 0x00) // one byte
        {
            /**
             * mapping rule: 0000 0000 - 0000 007F | 0xxxxxxx
             */
            // nothing to check
        } else if ((*iter & 0xe0) == 0xc0) // two bytes
        {
            /**
             * mapping rule: 0000 0080 - 0000 07FF | 110xxxxx 10xxxxxx
             */
            c = *iter;
            FILL_BLUNK_AND_CONTINUE_IF_TRUE(iter + 1 == str.end());
            /* check whether the byte is in format 10xxxxxx */
            FILL_BLUNK_AND_CONTINUE_IF_TRUE((*(iter + 1) & UTF8_BYTE_MASK) != UTF8_BYTE_PREFIX);
            /* get unicode value */
            unicode = (((c & 0x1f) << 6) | (*(iter + 1) & 0x3f));
            /* validate unicode range */
            FILL_BLUNK_AND_CONTINUE_IF_TRUE(!(unicode >= 0x80 && unicode <= 0x7ff));
            ++iter;
        } else if ((*iter & 0xf0) == 0xe0) // three bytes
        {
            /**
             * mapping rule: 0000 0800 - 0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
             */
            c = *iter;
            FILL_BLUNK_AND_CONTINUE_IF_TRUE((iter + 1 == str.end()) || (iter + 2 == str.end()));
            /* check whether the byte is in format 10xxxxxx */
            FILL_BLUNK_AND_CONTINUE_IF_TRUE((*(iter + 1) & UTF8_BYTE_MASK) != UTF8_BYTE_PREFIX);
            FILL_BLUNK_AND_CONTINUE_IF_TRUE((*(iter + 2) & UTF8_BYTE_MASK) != UTF8_BYTE_PREFIX);
            /* get unicode value */
            unicode = (((c & 0x0f) << 12) | ((*(iter + 1) & 0x3f) << 6) | (*(iter + 2) & 0x3f));
            /* validate unicode range */
            FILL_BLUNK_AND_CONTINUE_IF_TRUE(!(unicode >= 0x800 /* && unicode <= 0xffff */));
            ++iter;
            ++iter;
        } else if ((*iter & 0xf8) == 0xf0) // four bytes
        {
            /**
             * mapping rule: 0001 0000 - 0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
             */
            c = *iter;
            FILL_BLUNK_AND_CONTINUE_IF_TRUE((iter + 1 == str.end()) || (iter + 2 == str.end())
                                            || (iter + 3 == str.end()));
            /* check whether the byte is in format 10xxxxxx */
            FILL_BLUNK_AND_CONTINUE_IF_TRUE((*(iter + 1) & UTF8_BYTE_MASK) != UTF8_BYTE_PREFIX);
            FILL_BLUNK_AND_CONTINUE_IF_TRUE((*(iter + 2) & UTF8_BYTE_MASK) != UTF8_BYTE_PREFIX);
            FILL_BLUNK_AND_CONTINUE_IF_TRUE((*(iter + 3) & UTF8_BYTE_MASK) != UTF8_BYTE_PREFIX);
            /* get unicode value */
            uint32_t unicode = 0x00000000; // for 4 bytes utf-8 encoding
            unicode = ((c & 0x07) << 18) | ((*(iter + 1) & 0x3f) << 12) | ((*(iter + 2) & 0x3f) << 6)
                | ((*(iter + 3) & 0x3f));
            /* validate unicode range */
            FILL_BLUNK_AND_CONTINUE_IF_TRUE(!(unicode >= 0x00010000 && unicode <= 0x0010ffff));
            ++iter;
            ++iter;
            ++iter;
        } else {
            FILL_BLUNK_AND_CONTINUE_IF_TRUE(true);
        }
        ++iter;
    }
#undef FILL_BLUNK_AND_CONTINUE_IF_TRUE
    return false;
}

} // namespace logtail