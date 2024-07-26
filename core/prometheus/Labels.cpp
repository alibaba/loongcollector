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

#include "prometheus/Labels.h"

#include <algorithm>
#include <cstdint>

using namespace std;
namespace logtail {

Labels::Labels() : mMetricEventPtr(nullptr) {
}

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
    } else {
        return "";
    }
}

/// @brief 从MetricEvent中获取Labels
/// @param metricEvent
/// @warning MetricEvent的name保存在__name__中
void Labels::Reset(MetricEvent* metricEvent) {
    // metricEventPtr = metricEvent;
    for (auto it = metricEvent->TagsBegin(); it != metricEvent->TagsEnd(); it++) {
        Push(Label(it->first.to_string(), it->second.to_string()));
    }
    Push(Label("__name__", metricEvent->GetName().to_string()));
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
    for (auto l : mLabels) {
        f(Label(l.first, l.second));
    }
}

LabelMap::const_iterator Labels::Begin() const {
    // if (metricEventPtr) {
    //     return metricEventPtr->LabelsBegin();
    // } else {
    return mLabels.begin();
    // }
}

LabelMap::const_iterator Labels::End() const {
    // if (metricEventPtr) {
    //     return metricEventPtr->LabelsEnd();
    // } else {
    return mLabels.end();
    // }
}


LabelsBuilder::LabelsBuilder() {
}

// Del deletes the label of the given name.
void LabelsBuilder::DeleteLabel(const vector<string>& nameList) {
    for (auto name : nameList) {
        DeleteLabel(name);
    }
}

void LabelsBuilder::DeleteLabel(std::string name) {
    auto it = find_if(mAddLabelList.begin(), mAddLabelList.end(), [&name](Label l) { return l.name == name; });
    if (it != mAddLabelList.end()) {
        mAddLabelList.erase(it);
    }
    mDeleteLabelNameList.emplace_back(name);
}

std::string LabelsBuilder::Get(const std::string& name) {
    // Del() removes entries from .add but Set() does not remove from .del, so check .add first.
    for (const Label& label : mAddLabelList) {
        if (label.name == name) {
            return label.value;
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
    for (size_t i = 0; i < mAddLabelList.size(); ++i) {
        if (mAddLabelList[i].name == name) {
            mAddLabelList[i].value = value;
            return;
        }
    }
    mAddLabelList.push_back(Label(name, value));
}

void LabelsBuilder::Reset(Labels l) {
    mBase = l;
    mBase.Range([this](Label l) {
        if (l.value == "") {
            mDeleteLabelNameList.push_back(l.name);
        }
    });
}

void LabelsBuilder::Reset(MetricEvent* metricEvent) {
    mBase.Reset(metricEvent);
    mBase.Range([this](Label l) {
        if (l.value == "") {
            mDeleteLabelNameList.push_back(l.name);
        }
    });
}

Labels LabelsBuilder::GetLabels() {
    if (mDeleteLabelNameList.empty() && mAddLabelList.empty()) {
        return mBase;
    }

    auto res = Labels();
    for (auto l = mBase.Begin(); l != mBase.End(); ++l) {
        if (find_if(mDeleteLabelNameList.begin(),
                    mDeleteLabelNameList.end(),
                    [&l](string name) { return name == l->first; })
                != mDeleteLabelNameList.end()
            || find_if(mAddLabelList.begin(), mAddLabelList.end(), [&l](Label label) { return label.name == l->first; })
                != mAddLabelList.end()) {
            continue;
        }
        res.Push(Label(l->first, l->second));
    }

    for (auto l : mAddLabelList) {
        res.Push(l);
    }
    // Sort res labels
    // Labels类在push_back后自动排序
    // sort(res.begin(), res.end(), [](Label a, Label b) { return a.name < b.name; });
    return res;
}


/// @brief Range calls f on each label in the Builder
void LabelsBuilder::Range(const std::function<void(Label)>& closure) {
    // Take a copy of add and del, so they are unaffected by calls to Set() or Del().
    auto originAdd = mAddLabelList;
    auto originDel = mDeleteLabelNameList;
    mBase.Range([&originAdd, &originDel, &closure](Label l) {
        if (find_if(originAdd.begin(), originAdd.end(), [&l](Label label) { return l.name == label.name; })
                == originAdd.end()
            && find(originDel.begin(), originDel.end(), l.name) == originDel.end()) {
            closure(l);
        }
    });
    for (auto item : originAdd) {
        closure(item);
    }
}

uint64_t Labels::Hash() {
    string hash = "";
    uint64_t sum = offset64;
    Range([&hash](Label l) { hash += l.name + "\xff" + l.value + "\xff"; });
    for (auto i : hash) {
        sum ^= (uint64_t)i;
        sum *= prime64;
    }
    return sum;
}

void Labels::RemoveMetaLabels() {
    for (auto it = mLabels.begin(); it != mLabels.end();) {
        if (it->first.find("__meta_") == 0) {
            it = mLabels.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace logtail
