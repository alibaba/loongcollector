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

#include "LogFilter.h"
#include <re2/re2.h>
#include "profiler/LogtailAlarm.h"
#include "app_config/AppConfig.h"
#include "common/util.h"
#include "sdk/Common.h"
#include "logger/Logger.h"
#include "config_manager/ConfigManager.h"

using namespace std;
using namespace sls_logs;

namespace logtail {

bool LogFilter::InitFilter(const string& configName) {
    Json::Value jsonRoot; // will contains the root value after parsing.
    ParseConfResult userLogRes = ParseConfig(configName, jsonRoot);
    if (userLogRes != CONFIG_OK) {
        if (userLogRes == CONFIG_NOT_EXIST)
            LOG_DEBUG(sLogger, (configName, "not found, uninitialized Filter"));
        if (userLogRes == CONFIG_INVALID_FORMAT)
            LOG_ERROR(sLogger, ("load user config for filter fail, file content is not valid json", configName));
        return false;
    }

    if (jsonRoot.isMember("filters") == false) {
        return true;
    }
    const Json::Value& filters = jsonRoot["filters"];
    uint32_t filterSize = filters.size();
    try {
        for (uint32_t index = 0; index < filterSize; index++) {
            const Json::Value& item = filters[index];
            string projectName = item["project_name"].asString();
            string category = item["category"].asString();
            const Json::Value& keys = item["keys"];
            const Json::Value& regs = item["regs"];
            if (keys.size() != regs.size()) {
                LOG_ERROR(sLogger,
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
        LOG_ERROR(sLogger, ("Can't parse the filter config", ""));
        return false;
    }
    return true;
}

vector<int32_t> LogFilter::Filter(const std::string& projectName, const std::string& region, const LogGroup& logGroup) {
    vector<int32_t> result;
    int32_t log_size = logGroup.logs_size();
    if (log_size == 0) {
        return result;
    }
    string key(projectName + "_" + logGroup.category());
    std::unordered_map<std::string, LogFilterRule*>::iterator it = mFilters.find(key);
    if (it == mFilters.end()) {
        // the resize is used to allocate the needed space at the begin
        // as a result, there will be log_size empty slot in result vector
        result.resize(log_size);
        for (int32_t i = 0; i < log_size; i++) {
            result[i] = i;
        }
        return result;
    }

    const LogFilterRule& rule = *(it->second);
    LogGroupContext context(region, projectName, logGroup.category());
    for (int32_t i = 0; i < log_size; i++) {
        if (IsMatched(logGroup.logs(i), rule, context)) {
            result.push_back(i);
        }
    }
    return result;
}

std::vector<int32_t>
LogFilter::Filter(const LogGroup& logGroup, const LogFilterRule* filterRule, const LogGroupContext& context) {
    vector<int32_t> result;
    int32_t log_size = logGroup.logs_size();
    if (log_size == 0) {
        return result;
    }
    if (filterRule == NULL) {
        result.resize(log_size);
        for (int32_t i = 0; i < log_size; i++) {
            result[i] = i;
        }
        return result;
    }

    for (int32_t i = 0; i < log_size; i++) {
        try {
            if (IsMatched(logGroup.logs(i), *filterRule, context)) {
                result.push_back(i);
            }
        } catch (...) {
            LOG_ERROR(sLogger, ("filter error ", i));
        }
    }
    return result;
}

std::vector<int32_t>
LogFilter::Filter(const sls_logs::LogGroup& logGroup, BaseFilterNodePtr node, const LogGroupContext& context) {
    std::vector<int32_t> index;

    // null node, all logs are passed
    if (node.get() == NULL) {
        index.resize(logGroup.logs_size());
        for (int i = 0; i < logGroup.logs_size(); ++i) {
            index[i] = i;
        }
        return index;
    }

    for (int i = 0; i < logGroup.logs_size(); ++i) {
        const sls_logs::Log& log = logGroup.logs(i);
        if (node->Match(log, context)) {
            index.push_back(i);
        }
    }
    return index;
}

bool LogFilter::IsMatched(const Log& log, const LogFilterRule& rule, const LogGroupContext& context) {
    const std::vector<std::string>& keys = rule.FilterKeys;
    const std::vector<boost::regex> regs = rule.FilterRegs;
    string exception;
    for (uint32_t i = 0; i < keys.size(); i++) {
        bool found = false;
        for (int j = 0; j < log.contents_size(); j++) {
            const Log_Content& content = log.contents(j);
            const string& key = content.key();
            const string& value = content.value();
            if (key == keys[i]) {
                found = true;
                exception.clear();
                if (!BoostRegexMatch(value.c_str(), regs[i], exception)) {
                    if (!exception.empty()) {
                        LOG_ERROR(sLogger, ("regex_match in Filter fail", exception));

                        if (LogtailAlarm::GetInstance()->IsLowLevelAlarmValid()) {
                            context.SendAlarm(REGEX_MATCH_ALARM, "regex_match in Filter fail:" + exception);
                        }
                    }
                    return false;
                }
                break;
            }
        }
        if (false == found) {
            return false;
        }
    }
    return true;
}

void LogFilter::CastSensitiveWords(sls_logs::LogGroup& logGroup, const Config* pConfig) {
    if (pConfig->mSensitiveWordCastOptions.empty() || logGroup.logs_size() == 0) {
        return;
    }
    int32_t logSize = logGroup.logs_size();
    for (int32_t i = 0; i < logSize; ++i) {
        sls_logs::Log* pLog = logGroup.mutable_logs(i);
        int32_t contentSize = pLog->contents_size();
        for (int32_t j = 0; j < contentSize; ++j) {
            sls_logs::Log_Content* pContent = pLog->mutable_contents(j);
            CastOneSensitiveWord(pContent, pConfig);
        }
    }
}

void LogFilter::CastOneSensitiveWord(sls_logs::Log_Content* pContent, const Config* pConfig) {
    const string& key = pContent->key();
    std::unordered_map<std::string, std::vector<SensitiveWordCastOption> >::const_iterator findRst
        = pConfig->mSensitiveWordCastOptions.find(key);
    if (findRst == pConfig->mSensitiveWordCastOptions.end()) {
        return;
    }
    const std::vector<SensitiveWordCastOption>& optionVec = findRst->second;
    string* pVal = pContent->mutable_value();
    for (size_t i = 0; i < optionVec.size(); ++i) {
        const SensitiveWordCastOption& opt = optionVec[i];
        if (!opt.mRegex || !opt.mRegex->ok()) {
            continue;
        }
        bool rst = false;

        if (opt.option == SensitiveWordCastOption::CONST_OPTION) {
            if (opt.replaceAll) {
                rst = RE2::GlobalReplace(pVal, *(opt.mRegex), opt.constValue);
            } else {
                rst = RE2::Replace(pVal, *(opt.mRegex), opt.constValue);
            }
        } else {
            re2::StringPiece srcStr(*pVal);
            size_t maxSize = pVal->size();
            size_t beginPos = 0;
            rst = true;
            string destStr;
            do {
                re2::StringPiece findRst;
                if (!re2::RE2::FindAndConsume(&srcStr, *(opt.mRegex), &findRst)) {
                    if (beginPos == (size_t)0) {
                        rst = false;
                    }
                    break;
                }
                // like  xxxx, psw=123abc,xx
                size_t beginOffset = findRst.data() + findRst.size() - pVal->data();
                size_t endOffset = srcStr.empty() ? maxSize : srcStr.data() - pVal->data();
                if (beginOffset < beginPos || endOffset <= beginPos || endOffset > maxSize) {
                    rst = false;
                    break;
                }
                // add : xxxx, psw
                destStr.append(pVal->substr(beginPos, beginOffset - beginPos));
                // md5: 123abc
                destStr.append(sdk::CalcMD5(pVal->substr(beginOffset, endOffset - beginOffset)));
                beginPos = endOffset;
                // refine for  : xxxx. psw=123abc
                if (endOffset >= maxSize) {
                    break;
                }

            } while (opt.replaceAll);

            if (rst && beginPos < pVal->size()) {
                // add ,xx
                destStr.append(pVal->substr(beginPos));
            }
            if (rst) {
                pContent->set_value(destStr);
                pVal = pContent->mutable_value();
            }
        }

        //if (!rst)
        //{
        //    LOG_WARNING(sLogger, ("cast sensitive word fail", opt.constValue)(pConfig->mProjectName, pConfig->mCategory));
        //    LogtailAlarm::GetInstance()->SendAlarm(CAST_SENSITIVE_WORD_ALARM, "cast sensitive word fail", pConfig->mProjectName, pConfig->mCategory, pConfig->mRegion);
        //}
    }
}

static const char UTF8_BYTE_PREFIX = 0x80;
static const char UTF8_BYTE_MASK = 0xc0;
void FilterNoneUtf8(const string& strSrc) {
    string* str = const_cast<string*>(&strSrc);
#define FILL_BLUNK_AND_CONTINUE_IF_TRUE(stat) \
    if (stat) { \
        *iter = ' '; \
        ++iter; \
        continue; \
    };
    string::iterator iter = str->begin();
    while (iter != str->end()) {
        uint16_t unicode = 0;
        char c;
        if ((*iter & 0x80) == 0x00) // one byte
        {
            /**
             * mapping rule: 0000 0000 - 0000 007F | 0xxxxxxx
             */
            //nothing to check
        } else if ((*iter & 0xe0) == 0xc0) // two bytes
        {
            /**
             * mapping rule: 0000 0080 - 0000 07FF | 110xxxxx 10xxxxxx
             */
            c = *iter;
            FILL_BLUNK_AND_CONTINUE_IF_TRUE(iter + 1 == str->end());
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
            FILL_BLUNK_AND_CONTINUE_IF_TRUE((iter + 1 == str->end()) || (iter + 2 == str->end()));
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
            FILL_BLUNK_AND_CONTINUE_IF_TRUE((iter + 1 == str->end()) || (iter + 2 == str->end())
                                            || (iter + 3 == str->end()));
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
}

} // namespace logtail
