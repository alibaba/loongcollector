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

#pragma once
#include <json/json.h>

#include <boost/regex.hpp>
#include <string>

#include "prometheus/labels/Labels.h"

namespace logtail {

enum class Action {
    REPLACE,
    KEEP,
    DROP,
    KEEPEQUAL,
    DROPEQUAL,
    HASHMOD,
    LABELMAP,
    LABELDROP,
    LABELKEEP,
    LOWERCASE,
    UPPERCASE,
    UNDEFINED
};

const std::string& ActionToString(Action action);
Action StringToAction(std::string action);

class LabelName {
public:
    LabelName();
    LabelName(std::string);

    bool Validate();

    std::string mLabelName;

private:
};

class RelabelConfig {
public:
    RelabelConfig();
    RelabelConfig(const Json::Value&);

    bool Validate();

    // A list of labels from which values are taken and concatenated
    // with the configured separator in order.
    std::vector<std::string> mSourceLabels;
    // Separator is the string between concatenated values from the source labels.
    std::string mSeparator;
    // Regex against which the concatenation is matched.
    boost::regex mRegex;
    // Modulus to take of the hash of concatenated values from the source labels.
    uint64_t mModulus = 0;
    // TargetLabel is the label to which the resulting string is written in a replacement.
    // Regexp interpolation is allowed for the replace action.
    std::string mTargetLabel;
    // Replacement is the regex replacement pattern to be used.
    std::string mReplacement;
    // Action is the action to be performed for the relabeling.
    Action mAction;

private:
};


namespace prometheus {
    bool Process(const Labels& lbls, const std::vector<RelabelConfig>& cfgs, Labels& ret);
    bool ProcessBuilder(LabelsBuilder& lb, const std::vector<RelabelConfig>& cfgs);
    bool Relabel(const RelabelConfig& cfg, LabelsBuilder& lb);
} // namespace prometheus


} // namespace logtail
