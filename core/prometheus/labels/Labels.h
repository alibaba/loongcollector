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

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "models/MetricEvent.h"

namespace logtail {

// Label is a key/value pair of strings.
struct Label {
    std::string name;
    std::string value;
    Label(std::string name, std::string value) : name(std::move(name)), value(std::move(value)) {}
};

using LabelMap = std::map<std::string, std::string>;
/// @brief Labels is a sorted set of labels. Order has to be guaranteed upon instantiation
class Labels {
public:
    Labels() = default;
    size_t Size() const;
    uint64_t Hash();
    void RemoveMetaLabels();

    std::string Get(const std::string&);
    void Reset(MetricEvent*);
    void Push(const Label&);

    void Range(const std::function<void(Label)>&);

    LabelMap::const_iterator Begin() const;
    LabelMap::const_iterator End() const;

private:
    LabelMap mLabels;

    MetricEvent* mMetricEventPtr = nullptr;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class LabelsUnittest;
#endif
};

class LabelsBuilder {
public:
    LabelsBuilder();
    void DeleteLabel(const std::vector<std::string>&);
    void DeleteLabel(std::string);

    std::string Get(const std::string&);
    void Set(const std::string&, const std::string&);

    void Reset(Labels);
    void Reset(MetricEvent*);

    Labels GetLabels();

    void Range(const std::function<void(Label)>& closure);

private:
    Labels mBase;

    std::unordered_set<std::string> mDeleteLabelNameList;
    std::unordered_map<std::string, std::string> mAddLabelList;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class LabelsBuilderUnittest;
#endif
};

} // namespace logtail
