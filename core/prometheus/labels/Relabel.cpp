/*
 * Copyright 2024 iLogtail Authors
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

#include "Relabel.h"

#include <openssl/md5.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/regex.hpp>
#include <string>
#include <vector>

#include "common/ParamExtractor.h"
#include "common/StringTools.h"
#include "logger/Logger.h"
#include "prometheus/Constants.h"

using namespace std;

#define ENUM_TO_STRING_CASE(EnumValue) \
    { Action::EnumValue, ToLowerCaseString(#EnumValue) }

#define STRING_TO_ENUM_CASE(EnumValue) \
    { ToLowerCaseString(#EnumValue), Action::EnumValue }

namespace logtail {

Action StringToAction(const string& action) {
    static std::map<string, Action> sActionStrings{STRING_TO_ENUM_CASE(REPLACE),
                                                   STRING_TO_ENUM_CASE(KEEP),
                                                   STRING_TO_ENUM_CASE(DROP),
                                                   STRING_TO_ENUM_CASE(KEEPEQUAL),
                                                   STRING_TO_ENUM_CASE(DROPEQUAL),
                                                   STRING_TO_ENUM_CASE(HASHMOD),
                                                   STRING_TO_ENUM_CASE(LABELMAP),
                                                   STRING_TO_ENUM_CASE(LABELDROP),
                                                   STRING_TO_ENUM_CASE(LABELKEEP),
                                                   STRING_TO_ENUM_CASE(LOWERCASE),
                                                   STRING_TO_ENUM_CASE(UPPERCASE)};

    auto it = sActionStrings.find(action);
    if (it != sActionStrings.end()) {
        return it->second;
    }
    return Action::UNDEFINED;
}

const std::string& ActionToString(Action action) {
    static std::map<Action, std::string> sActionStrings{ENUM_TO_STRING_CASE(REPLACE),
                                                        ENUM_TO_STRING_CASE(KEEP),
                                                        ENUM_TO_STRING_CASE(DROP),
                                                        ENUM_TO_STRING_CASE(KEEPEQUAL),
                                                        ENUM_TO_STRING_CASE(DROPEQUAL),
                                                        ENUM_TO_STRING_CASE(HASHMOD),
                                                        ENUM_TO_STRING_CASE(LABELMAP),
                                                        ENUM_TO_STRING_CASE(LABELDROP),
                                                        ENUM_TO_STRING_CASE(LABELKEEP),
                                                        ENUM_TO_STRING_CASE(LOWERCASE),
                                                        ENUM_TO_STRING_CASE(UPPERCASE)};
    static string sUndefined = prometheus::UNDEFINED;
    auto it = sActionStrings.find(action);
    if (it != sActionStrings.end()) {
        return it->second;
    }
    return sUndefined;
}
RelabelConfig::RelabelConfig() : mSeparator(";"), mReplacement("$1"), mAction(Action::REPLACE) {
    mRegex = boost::regex("().*");
}
bool RelabelConfig::Init(const Json::Value& config) {
    string errorMsg;

    if (config.isMember(prometheus::SOURCE_LABELS) && config[prometheus::SOURCE_LABELS].isArray()) {
        GetOptionalListParam<string>(config, prometheus::SOURCE_LABELS, mSourceLabels, errorMsg);
    }

    if (config.isMember(prometheus::SEPARATOR) && config[prometheus::SEPARATOR].isString()) {
        mSeparator = config[prometheus::SEPARATOR].asString();
    }

    if (config.isMember(prometheus::TARGET_LABEL) && config[prometheus::TARGET_LABEL].isString()) {
        mTargetLabel = config[prometheus::TARGET_LABEL].asString();
    }

    if (config.isMember(prometheus::REGEX) && config[prometheus::REGEX].isString()) {
        string re = config[prometheus::REGEX].asString();
        mRegex = boost::regex(re);
    }

    if (config.isMember(prometheus::REPLACEMENT) && config[prometheus::REPLACEMENT].isString()) {
        mReplacement = config[prometheus::REPLACEMENT].asString();
    }

    if (config.isMember(prometheus::ACTION) && config[prometheus::ACTION].isString()) {
        string actionString = config[prometheus::ACTION].asString();
        mAction = StringToAction(actionString);
    }

    if (config.isMember(prometheus::MODULUS) && config[prometheus::MODULUS].isUInt64()) {
        mModulus = config[prometheus::MODULUS].asUInt64();
    }
    return true;
}

bool RelabelConfig::Process(Labels& l) const {
    vector<string> values;
    values.reserve(mSourceLabels.size());
    for (const auto& item : mSourceLabels) {
        values.push_back(l.Get(item));
    }
    string val = boost::algorithm::join(values, mSeparator);

    switch (mAction) {
        case Action::DROP: {
            if (boost::regex_match(val, mRegex)) {
                return false;
            }
            break;
        }
        case Action::KEEP: {
            if (!boost::regex_match(val, mRegex)) {
                return false;
            }
            break;
        }
        case Action::DROPEQUAL: {
            if (l.Get(mTargetLabel) == val) {
                return false;
            }
            break;
        }
        case Action::KEEPEQUAL: {
            if (l.Get(mTargetLabel) != val) {
                return false;
            }
            break;
        }
        case Action::REPLACE: {
            bool indexes = boost::regex_search(val, mRegex);
            // If there is no match no replacement must take place.
            if (!indexes) {
                break;
            }
            string target = string(boost::regex_replace(val, mRegex, mTargetLabel, boost::format_first_only));
            string res = boost::regex_replace(val, mRegex, mReplacement, boost::format_first_only);
            if (res.size() == 0) {
                l.Del(target);
                break;
            }
            l.Set(target, string(res));
            break;
        }
        case Action::LOWERCASE: {
            l.Set(mTargetLabel, boost::to_lower_copy(val));
            break;
        }
        case Action::UPPERCASE: {
            l.Set(mTargetLabel, boost::to_upper_copy(val));
            break;
        }
        case Action::HASHMOD: {
            uint8_t digest[MD5_DIGEST_LENGTH];
            MD5((uint8_t*)val.c_str(), val.length(), (uint8_t*)&digest);
            // Use only the last 8 bytes of the hash to give the same result as earlier versions of this code.
            uint64_t hashVal = 0;
            for (int i = 8; i < MD5_DIGEST_LENGTH; ++i) {
                hashVal = (hashVal << 8) | digest[i];
            }
            uint64_t mod = hashVal % mModulus;
            l.Set(mTargetLabel, to_string(mod));
            break;
        }
        case Action::LABELMAP: {
            l.Range([&](const string& key, const string& value) {
                if (boost::regex_match(key, mRegex)) {
                    string res
                        = boost::regex_replace(key, mRegex, mReplacement, boost::match_default | boost::format_all);
                    l.Set(res, value);
                }
            });
            break;
        }
        case Action::LABELDROP: {
            vector<string> toDel;
            l.Range([&](const string& key, const string& value) {
                if (boost::regex_match(key, mRegex)) {
                    toDel.push_back(key);
                }
            });
            for (const auto& item : toDel) {
                l.Del(item);
            }
            break;
        }
        case Action::LABELKEEP: {
            vector<string> toDel;
            l.Range([&](const string& key, const string& value) {
                if (!boost::regex_match(key, mRegex)) {
                    toDel.push_back(key);
                }
            });
            for (const auto& item : toDel) {
                l.Del(item);
            }
            break;
        }
        default:
            // error
            LOG_ERROR(sLogger, ("relabel: unknown relabel action type", ActionToString(mAction)));
            break;
    }
    return true;
}

bool RelabelConfigList::Init(const Json::Value& relabelConfigs) {
    if (!relabelConfigs.isArray()) {
        return false;
    }
    for (const auto& relabelConfig : relabelConfigs) {
        RelabelConfig rc;
        if (rc.Init(relabelConfig)) {
            mRelabelConfigs.push_back(rc);
        } else {
            return false;
        }
    }
    return true;
}

bool RelabelConfigList::Process(Labels& l) const {
    for (const auto& cfg : mRelabelConfigs) {
        if (!cfg.Process(l)) {
            return false;
        }
    }
    return true;
}

bool RelabelConfigList::Process(MetricEvent& event) const {
    Labels labels;
    labels.Reset(&event);
    return Process(labels);
}

bool RelabelConfigList::Empty() const {
    return mRelabelConfigs.empty();
}

} // namespace logtail
