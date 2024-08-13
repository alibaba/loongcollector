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

#include "prometheus/labels/Labels.h"

#include <algorithm>
#include <cstdint>

#include "prometheus/Constants.h"

using namespace std;
namespace logtail {


size_t Labels::Size() const {
    if (mMetricEventPtr) {
        return mMetricEventPtr->TagsSize();
    }
    return mLabels.size();
}

std::string Labels::Get(const string& name) {
    if (mMetricEventPtr) {
        return string(mMetricEventPtr->GetTag(name));
    }
    if (mLabels.count(name)) {
        return mLabels[name];
    }
    return "";
}

void Labels::Reset(MetricEvent* metricEvent) {
    for (auto it = metricEvent->TagsBegin(); it != metricEvent->TagsEnd(); it++) {
        Push(Label(it->first.to_string(), it->second.to_string()));
    }
    Push(Label(prometheus::NAME, metricEvent->GetName().to_string()));
}

void Labels::Push(const Label& l) {
    if (mMetricEventPtr) {
        mMetricEventPtr->SetTag(l.name, l.value);
        return;
    }
    mLabels[l.name] = l.value;
}

void Labels::Range(const std::function<void(Label)>& f) {
    if (mMetricEventPtr) {
        for (auto l = mMetricEventPtr->TagsBegin(); l != mMetricEventPtr->TagsEnd(); l++) {
            f(Label(string(l->first), string(l->second)));
        }
        return;
    }
    for (const auto& l : mLabels) {
        f(Label(l.first, l.second));
    }
}

LabelMap::const_iterator Labels::Begin() const {
    return mLabels.begin();
}

LabelMap::const_iterator Labels::End() const {
    return mLabels.end();
}


LabelsBuilder::LabelsBuilder() {
}

// Del deletes the label of the given name.
void LabelsBuilder::DeleteLabel(const vector<string>& nameList) {
    for (const auto& name : nameList) {
        DeleteLabel(name);
    }
}

void LabelsBuilder::DeleteLabel(std::string name) {
    auto it = mAddLabelList.find(name);
    if (it != mAddLabelList.end()) {
        mAddLabelList.erase(it);
    }
    mDeleteLabelNameList.insert(name);
}

std::string LabelsBuilder::Get(const std::string& name) {
    // Del() removes entries from .add but Set() does not remove from .del, so check .add first.
    for (const auto& [k, v] : mAddLabelList) {
        if (k == name) {
            return v;
        }
    }
    auto it = find(mDeleteLabelNameList.begin(), mDeleteLabelNameList.end(), name);
    if (it != mDeleteLabelNameList.end()) {
        return "";
    }
    return mBase.Get(name);
}


// Set the name/value pair as a label. A value of "" means delete that label.
void LabelsBuilder::Set(const std::string& name, const std::string& value) {
    if (value.empty()) {
        DeleteLabel(name);
        return;
    }
    if (mAddLabelList.find(name) != mAddLabelList.end()) {
        mAddLabelList[name] = value;
        return;
    }
    mAddLabelList.emplace(name, value);
}

void LabelsBuilder::Reset(Labels l) {
    mBase = l;
    mBase.Range([this](const Label& l) {
        if (l.value == "") {
            mDeleteLabelNameList.insert(l.name);
        }
    });
}

void LabelsBuilder::Reset(MetricEvent* metricEvent) {
    mBase.Reset(metricEvent);
    mBase.Range([this](const Label& l) {
        if (l.value == "") {
            mDeleteLabelNameList.insert(l.name);
        }
    });
}

Labels LabelsBuilder::GetLabels() {
    if (mDeleteLabelNameList.empty() && mAddLabelList.empty()) {
        return mBase;
    }

    auto res = Labels();
    for (auto l = mBase.Begin(); l != mBase.End(); ++l) {
        if (mDeleteLabelNameList.find(l->first) != mDeleteLabelNameList.end()
            || mAddLabelList.find(l->first) != mAddLabelList.end()) {
            continue;
        }
        res.Push(Label(l->first, l->second));
    }

    for (const auto& [k, v] : mAddLabelList) {
        res.Push(Label{k, v});
    }

    return res;
}


/// @brief Range calls f on each label in the Builder
void LabelsBuilder::Range(const std::function<void(Label)>& closure) {
    // Take a copy of add and del, so they are unaffected by calls to Set() or Del().
    auto originAdd = mAddLabelList;
    auto originDel = mDeleteLabelNameList;
    mBase.Range([&originAdd, &originDel, &closure](const Label& l) {
        if (originAdd.find(l.name) == originAdd.end() && originDel.find(l.name) == originDel.end()) {
            closure(l);
        }
    });
    for (const auto& [k, v] : originAdd) {
        closure(Label{k, v});
    }
}

uint64_t Labels::Hash() {
    string hash;
    uint64_t sum = prometheus::OFFSET64;
    Range([&hash](const Label& l) { hash += l.name + "\xff" + l.value + "\xff"; });
    for (auto i : hash) {
        sum ^= (uint64_t)i;
        sum *= prometheus::PRIME64;
    }
    return sum;
}

void Labels::RemoveMetaLabels() {
    for (auto it = mLabels.begin(); it != mLabels.end();) {
        if (it->first.find(prometheus::META) == 0) {
            it = mLabels.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace logtail
