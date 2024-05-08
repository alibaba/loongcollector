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

#include <vector>

#include "common/ParamExtractor.h"
#include "logger/Logger.h"
#include "models/LogEvent.h"
#include "monitor/MetricConstants.h"

namespace logtail {

const std::string ProcessorFilterNative::sName = "processor_filter_regex_native";

bool ProcessorFilterNative::Init(const Json::Value& config) {
    std::string errorMsg;

    // FilterKey + FilterRegex
    std::vector<std::string> filterKeys, filterRegs;
    if (!GetOptionalListParam(config, "FilterKey", filterKeys, errorMsg)
        || !GetOptionalListParam(config, "FilterRegex", filterRegs, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    } else if (filterKeys.size() != filterRegs.size()) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             "param FilterKey and FilterRegex does not have the same size",
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    } else if (!filterKeys.empty()) {
        bool hasError = false;
        std::vector<boost::regex> regs;
        for (const auto& reg : filterRegs) {
            if (!IsRegexValid(reg)) {
                PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                     mContext->GetAlarm(),
                                     "value in list param FilterRegex is not a valid regex",
                                     sName,
                                     mContext->GetConfigName(),
                                     mContext->GetProjectName(),
                                     mContext->GetLogstoreName(),
                                     mContext->GetRegion());
                hasError = true;
                break;
            }
            regs.emplace_back(reg);
        }
        if (!hasError) {
            mFilterRule = std::make_shared<LogFilterRule>();
            mFilterRule->FilterKeys = filterKeys;
            mFilterRule->FilterRegs = regs;
            mFilterMode = Mode::RULE_MODE;
        }
    }

    // TODO: deprecated, remove in the future
    // Include
    if (mFilterMode == Mode::BYPASS_MODE) {
        if (!GetOptionalMapParam(config, "Include", mInclude, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        } else if (!mInclude.empty()) {
            std::vector<std::string> keys;
            std::vector<boost::regex> regs;
            bool hasError = false;
            for (auto& include : mInclude) {
                if (!IsRegexValid(include.second)) {
                    PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                         mContext->GetAlarm(),
                                         "value in map param Include is not a valid regex",
                                         sName,
                                         mContext->GetConfigName(),
                                         mContext->GetProjectName(),
                                         mContext->GetLogstoreName(),
                                         mContext->GetRegion());
                    hasError = true;
                    break;
                }
                keys.emplace_back(include.first);
                regs.emplace_back(boost::regex(include.second));
            }
            if (!hasError) {
                mFilterRule = std::make_shared<LogFilterRule>();
                mFilterRule->FilterKeys = keys;
                mFilterRule->FilterRegs = regs;
                mFilterMode = Mode::RULE_MODE;
            }
        }
    }

    // ConditionExp
    if (mFilterMode == Mode::BYPASS_MODE) {
        const char* key = "ConditionExp";
        const Json::Value* itr = config.find(key, key + strlen(key));
        if (itr == nullptr) {
            PARAM_ERROR_RETURN(mContext->GetLogger(),
                               mContext->GetAlarm(),
                               "param ConditionExp is missing",
                               sName,
                               mContext->GetConfigName(),
                               mContext->GetProjectName(),
                               mContext->GetLogstoreName(),
                               mContext->GetRegion());
        }
        if (!itr->isObject()) {
            PARAM_ERROR_RETURN(mContext->GetLogger(),
                               mContext->GetAlarm(),
                               "object param ConditionExp is not of type object",
                               sName,
                               mContext->GetConfigName(),
                               mContext->GetProjectName(),
                               mContext->GetLogstoreName(),
                               mContext->GetRegion());
        }
        BaseFilterNodePtr root = ParseExpressionFromJSON(*itr);
        if (!root) {
            PARAM_ERROR_RETURN(mContext->GetLogger(),
                               mContext->GetAlarm(),
                               "object param ConditionExp is not valid",
                               sName,
                               mContext->GetConfigName(),
                               mContext->GetProjectName(),
                               mContext->GetLogstoreName(),
                               mContext->GetRegion());
        }
        mConditionExp.swap(root);
        mFilterMode = Mode::EXPRESSION_MODE;
    }

    if (mFilterMode == Mode::BYPASS_MODE) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           "neither param Include nor param ConditionExp is valid",
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    // DiscardingNonUTF8
    if (!GetOptionalBoolParam(config, "DiscardingNonUTF8", mDiscardingNonUTF8, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mDiscardingNonUTF8,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    mProcFilterErrorTotal = GetMetricsRecordRef().CreateCounter(METRIC_PROC_FILTER_ERROR_TOTAL);
    mProcFilterRecordsTotal = GetMetricsRecordRef().CreateCounter(METRIC_PROC_FILTER_RECORDS_TOTAL);

    return true;
}

void ProcessorFilterNative::Process(PipelineEventGroup& logGroup) {
    if (logGroup.GetEvents().empty()) {
        return;
    }

    EventsContainer& events = logGroup.MutableEvents();

    size_t wIdx = 0;
    for (size_t rIdx = 0; rIdx < events.size(); ++rIdx) {
        if (ProcessEvent(events[rIdx])) {
            if (wIdx != rIdx) {
                events[wIdx] = std::move(events[rIdx]);
            }
            ++wIdx;
        } else {
            mProcFilterRecordsTotal->Add(1);
        }
    }
    events.resize(wIdx);
}

bool ProcessorFilterNative::ProcessEvent(PipelineEventPtr& e) {
    if (!IsSupportedEvent(e)) {
        return true;
    }

    auto& sourceEvent = e.Cast<LogEvent>();
    bool res = true;

    if (mFilterMode == Mode::EXPRESSION_MODE) {
        res = FilterExpressionRoot(sourceEvent, mConditionExp);
    } else if (mFilterMode == Mode::RULE_MODE) {
        res = FilterFilterRule(sourceEvent, mFilterRule.get());
    }
    if (res && mDiscardingNonUTF8) {
        std::vector<std::pair<StringView, StringView> > newContents;
        for (auto &content: sourceEvent) {
            if (CheckNoneUtf8(content.second)) {
                auto value = content.second.to_string();
                FilterNoneUtf8(value);
                StringBuffer valueBuffer = sourceEvent.GetSourceBuffer()->CopyString(value);
                content.second = StringView(valueBuffer.data, valueBuffer.size);
            }
            if (CheckNoneUtf8(content.first)) {
                // key
                auto key = content.first.to_string();
                FilterNoneUtf8(key);
                StringBuffer keyBuffer = sourceEvent.GetSourceBuffer()->CopyString(key);

                newContents.emplace_back(StringView(keyBuffer.data, keyBuffer.size), content.second);
                sourceEvent.DelContent(content.first);
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
    if (sourceEvent.Empty()) {
        return false;
    }

    // null node, all logs are passed
    if (node.get() == nullptr) {
        return true;
    }

    try {
        return node->Match(sourceEvent, GetContext());
    } catch (...) {
        mProcFilterErrorTotal->Add(1);
        LOG_ERROR(GetContext().GetLogger(), ("filter error ", ""));
        return false;
    }
}

bool ProcessorFilterNative::FilterFilterRule(LogEvent& sourceEvent, const LogFilterRule* filterRule) {
    if (sourceEvent.Empty()) {
        return false;
    }

    // null filterRule, all logs are passed
    if (filterRule == NULL) {
        return true;
    }

    try {
        return IsMatched(sourceEvent, *filterRule);
    } catch (...) {
        mProcFilterErrorTotal->Add(1);
        LOG_ERROR(GetContext().GetLogger(), ("filter error ", ""));
        return false;
    }
}

bool ProcessorFilterNative::IsMatched(const LogEvent& contents, const LogFilterRule& rule) {
    const std::vector<std::string>& keys = rule.FilterKeys;
    const std::vector<boost::regex> regs = rule.FilterRegs;
    std::string exception;
    for (uint32_t i = 0; i < keys.size(); ++i) {
        const auto& content = contents.FindContent(keys[i]);
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

BaseFilterNodePtr ParseExpressionFromJSON(const Json::Value& value) {
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
        // if "op" element occurs, "operands" element must exist its type must be array, otherwise we consider it as
        // invalid json
        const Json::Value& operandsValue = value["operands"];
        if (filterOperator == NOT_OPERATOR && operandsValue.size() == 1) {
            BaseFilterNodePtr childNode = ParseExpressionFromJSON(operandsValue[0]);
            if (childNode) {
                node.reset(new UnaryFilterOperatorNode(childNode));
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

bool GetOperatorType(const std::string& type, FilterOperator& op) {
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

bool GetNodeFuncType(const std::string& type, FilterNodeFunctionType& func) {
    if (type == "regex") {
        func = REGEX_FUNCTION;
    } else {
        LOG_ERROR(sLogger, ("invalid func type", type));
        return false;
    }
    return true;
}


bool BinaryFilterOperatorNode::Match(const sls_logs::Log& log, const LogGroupContext& context) {
    if (BOOST_LIKELY(left && right)) {
        if (op == AND_OPERATOR) {
            return left->Match(log, context) && right->Match(log, context);
        } else if (op == OR_OPERATOR) {
            return left->Match(log, context) || right->Match(log, context);
        }
    }
    return false;
}

bool BinaryFilterOperatorNode::Match(const LogEvent& contents, const PipelineContext& mContext) {
    if (BOOST_LIKELY(left && right)) {
        if (op == AND_OPERATOR) {
            return left->Match(contents, mContext) && right->Match(contents, mContext);
        } else if (op == OR_OPERATOR) {
            return left->Match(contents, mContext) || right->Match(contents, mContext);
        }
    }
    return false;
}


bool RegexFilterValueNode::Match(const sls_logs::Log& log, const LogGroupContext& context) {
    for (int i = 0; i < log.contents_size(); ++i) {
        const sls_logs::Log_Content& content = log.contents(i);
        if (content.key() != key) {
            continue;
        }

        std::string exception;
        bool result = BoostRegexMatch(content.value().c_str(), content.value().size(), reg, exception);
        if (!result && !exception.empty() && AppConfig::GetInstance()->IsLogParseAlarmValid()) {
            LOG_ERROR(sLogger, ("regex_match in Filter fail", exception));
            if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                context.SendAlarm(REGEX_MATCH_ALARM, "regex_match in Filter fail:" + exception);
            }
        }
        return result;
    }
    return false;
}


bool RegexFilterValueNode::Match(const LogEvent& contents, const PipelineContext& mContext) {
    const auto& content = contents.FindContent(key);
    if (content == contents.end()) {
        return false;
    }

    std::string exception;
    bool result = BoostRegexMatch(content->second.data(), content->second.size(), reg, exception);
    if (!result && !exception.empty() && AppConfig::GetInstance()->IsLogParseAlarmValid()) {
        LOG_ERROR(mContext.GetLogger(), ("regex_match in Filter fail", exception));
        if (mContext.GetAlarm().IsLowLevelAlarmValid()) {
            mContext.GetAlarm().SendAlarm(REGEX_MATCH_ALARM,
                                          "regex_match in Filter fail:" + exception,
                                          mContext.GetProjectName(),
                                          mContext.GetLogstoreName(),
                                          mContext.GetRegion());
        }
    }
    return result;
}


bool UnaryFilterOperatorNode::Match(const sls_logs::Log& log, const LogGroupContext& context) {
    if (BOOST_LIKELY(child.get() != NULL)) {
        return !child->Match(log, context);
    }
    return false;
}


bool UnaryFilterOperatorNode::Match(const LogEvent& contents, const PipelineContext& mContext) {
    if (BOOST_LIKELY(child.get() != NULL)) {
        return !child->Match(contents, mContext);
    }
    return false;
}

} // namespace logtail
