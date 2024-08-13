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

#include "common/ParamExtractor.h"
#include "common/StringTools.h"
#include "logger/Logger.h"
#include "prometheus/Constants.h"

using namespace std;

#define ENUM_TO_STRING_CASE(EnumValue) \
    { Action::EnumValue, ToLowerCaseString(#EnumValue) }

#define STRING_TO_ENUM__CASE(EnumValue) \
    { ToLowerCaseString(#EnumValue), Action::EnumValue }

namespace logtail {

Action StringToAction(string action) {
    static std::map<string, Action> actionStrings{STRING_TO_ENUM__CASE(REPLACE),
                                                  STRING_TO_ENUM__CASE(KEEP),
                                                  STRING_TO_ENUM__CASE(DROP),
                                                  STRING_TO_ENUM__CASE(KEEPEQUAL),
                                                  STRING_TO_ENUM__CASE(DROPEQUAL),
                                                  STRING_TO_ENUM__CASE(HASHMOD),
                                                  STRING_TO_ENUM__CASE(LABELMAP),
                                                  STRING_TO_ENUM__CASE(LABELDROP),
                                                  STRING_TO_ENUM__CASE(LABELKEEP),
                                                  STRING_TO_ENUM__CASE(LOWERCASE),
                                                  STRING_TO_ENUM__CASE(UPPERCASE)};

    auto it = actionStrings.find(action);
    if (it != actionStrings.end()) {
        return it->second;
    }
    return Action::UNDEFINED;
}

const std::string& ActionToString(Action action) {
    static std::map<Action, std::string> actionStrings{ENUM_TO_STRING_CASE(REPLACE),
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
    static string undefined = prometheus::UNDEFINED;
    auto it = actionStrings.find(action);
    if (it != actionStrings.end()) {
        return it->second;
    }
    return undefined;
}

RelabelConfig::RelabelConfig() {
}

RelabelConfig::RelabelConfig(const Json::Value& config) {
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
}

bool RelabelConfig::Validate() {
    return true;
}

bool prometheus::Process(const Labels& lbls, const std::vector<RelabelConfig>& cfgs, Labels& ret) {
    auto lb = LabelsBuilder();
    lb.Reset(lbls);
    if (!ProcessBuilder(lb, cfgs)) {
        ret = Labels();
        return false;
    }
    ret = lb.GetLabels();
    return true;
}

bool prometheus::ProcessBuilder(LabelsBuilder& lb, const std::vector<RelabelConfig>& cfgs) {
    for (const RelabelConfig& cfg : cfgs) {
        bool keep = Relabel(cfg, lb);
        if (!keep) {
            return false;
        }
    }
    return true;
}

bool prometheus::Relabel(const RelabelConfig& cfg, LabelsBuilder& lb) {
    vector<string> values;
    for (auto item : cfg.mSourceLabels) {
        values.push_back(lb.Get(item));
    }
    string val = boost::algorithm::join(values, cfg.mSeparator);

    switch (cfg.mAction) {
        case Action::DROP: {
            if (boost::regex_match(val, cfg.mRegex)) {
                return false;
            }
            break;
        }
        case Action::KEEP: {
            if (!boost::regex_match(val, cfg.mRegex)) {
                return false;
            }
            break;
        }
        case Action::DROPEQUAL: {
            if (lb.Get(cfg.mTargetLabel) == val) {
                return false;
            }
            break;
        }
        case Action::KEEPEQUAL: {
            if (lb.Get(cfg.mTargetLabel) != val) {
                return false;
            }
            break;
        }
        case Action::REPLACE: {
            bool indexes = boost::regex_search(val, cfg.mRegex);
            // If there is no match no replacement must take place.
            if (!indexes) {
                break;
            }
            LabelName target
                = LabelName(boost::regex_replace(val, cfg.mRegex, cfg.mTargetLabel, boost::format_first_only));
            if (!target.Validate()) {
                break;
            }
            string res = boost::regex_replace(val, cfg.mRegex, cfg.mReplacement, boost::format_first_only);
            if (res.size() == 0) {
                lb.DeleteLabel(target.mLabelName);
                break;
            }
            lb.Set(target.mLabelName, string(res));
            break;
        }
        case Action::LOWERCASE: {
            lb.Set(cfg.mTargetLabel, boost::to_lower_copy(val));
            break;
        }
        case Action::UPPERCASE: {
            lb.Set(cfg.mTargetLabel, boost::to_upper_copy(val));
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
            uint64_t mod = hashVal % cfg.mModulus;
            lb.Set(cfg.mTargetLabel, to_string(mod));
            break;
        }
        case Action::LABELMAP: {
            lb.Range([&cfg, &lb](Label label) {
                if (boost::regex_match(label.name, cfg.mRegex)) {
                    string res = boost::regex_replace(
                        label.name, cfg.mRegex, cfg.mReplacement, boost::match_default | boost::format_all);
                    lb.Set(res, label.value);
                }
            });
            break;
        }
        case Action::LABELDROP: {
            lb.Range([&cfg, &lb](Label label) {
                if (boost::regex_match(label.name, cfg.mRegex)) {
                    lb.DeleteLabel(label.name);
                }
            });
            break;
        }
        case Action::LABELKEEP: {
            lb.Range([&cfg, &lb](Label label) {
                if (!boost::regex_match(label.name, cfg.mRegex)) {
                    lb.DeleteLabel(label.name);
                }
            });
            break;
        }
        default:
            // error
            LOG_ERROR(sLogger, ("relabel: unknown relabel action type", ActionToString(cfg.mAction)));
            break;
    }
    return true;
}
LabelName::LabelName() {
}
LabelName::LabelName(std::string labelName) : mLabelName(labelName) {
}

bool LabelName::Validate() {
    return true;
}
} // namespace logtail
