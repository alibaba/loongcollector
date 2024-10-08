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

#include <cstdint>

#include "prometheus/Constants.h"

using namespace std;
namespace logtail {


// mMetricEventPtr can not be copied
Labels::Labels(const Labels& other) : mLabels(other.mLabels) {
}

Labels& Labels::operator=(const Labels& other) {
    if (this != &other) {
        mLabels = other.mLabels;
        mMetricEventPtr = nullptr;
    }
    return *this;
}

// metricEventPtr can be moved
Labels::Labels(Labels&& other) noexcept : mLabels(std::move(other.mLabels)), mMetricEventPtr(other.mMetricEventPtr) {
    other.mLabels.clear();
    other.mMetricEventPtr = nullptr;
}

Labels& Labels::operator=(Labels&& other) noexcept {
    if (this != &other) {
        mLabels = std::move(other.mLabels);
        mMetricEventPtr = other.mMetricEventPtr;
        other.mLabels.clear();
        other.mMetricEventPtr = nullptr;
    }
    return *this;
}


size_t Labels::Size() const {
    if (mMetricEventPtr) {
        return mMetricEventPtr->TagsSize();
    }
    return mLabels.size();
}

std::string Labels::Get(const string& name) {
    if (mMetricEventPtr) {
        return mMetricEventPtr->GetTag(name).to_string();
    }
    if (mLabels.count(name)) {
        return mLabels[name];
    }
    return "";
}

void Labels::Reset(MetricEvent* metricEvent) {
    mMetricEventPtr = metricEvent;
    Set(prometheus::NAME, metricEvent->GetName().to_string());
}

void Labels::Set(const string& k, const string& v) {
    if (mMetricEventPtr) {
        mMetricEventPtr->SetTag(k, v);
        return;
    }
    mLabels[k] = v;
}

void Labels::Del(const string& k) {
    if (mMetricEventPtr) {
        if(mMetricEventPtr->HasTag(k)){
            mMetricEventPtr->DelTag(k);
        }
        return;
    }
    if(mLabels.count(k)){
        mLabels.erase(k);
    }
}


void Labels::Range(const std::function<void(const string& k, const string& v)>& f) {
    if (mMetricEventPtr) {
        for (auto l = mMetricEventPtr->TagsBegin(); l != mMetricEventPtr->TagsEnd(); l++) {
            f(l->first.to_string(), l->second.to_string());
        }
        return;
    }
    for (const auto& l : mLabels) {
        f(l.first, l.second);
    }
}

uint64_t Labels::Hash() {
    string hash;
    uint64_t sum = prometheus::OFFSET64;
    Range([&hash](const string& k, const string& v) { hash += k + "\xff" + v + "\xff"; });
    for (auto i : hash) {
        sum ^= (uint64_t)i;
        sum *= prometheus::PRIME64;
    }
    return sum;
}

void Labels::RemoveMetaLabels() {
    // for mLabels only
    for (auto it = mLabels.begin(); it != mLabels.end();) {
        if (it->first.find(prometheus::META) == 0) {
            it = mLabels.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace logtail
