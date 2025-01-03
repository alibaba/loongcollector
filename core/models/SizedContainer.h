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

#include <map>
#include <vector>

#include "models/StringView.h"

namespace logtail {

// TODO: Be a complete wrapper of the original container

class SizedMap {
public:
    void Insert(StringView key, StringView val) {
        auto iter = mInner.find(key);
        if (iter != mInner.end()) {
            mAllocatedSize += val.size() - iter->second.size();
            iter->second = val;
        } else {
            mAllocatedSize += key.size() + val.size();
            mInner[key] = val;
        }
    }

    void Erase(StringView key) {
        auto iter = mInner.find(key);
        if (iter != mInner.end()) {
            mAllocatedSize -= iter->first.size() + iter->second.size();
            mInner.erase(iter);
        }
    }

    size_t DataSize() const { return sizeof(decltype(mInner)) + mAllocatedSize; }

    void Clear() {
        mInner.clear();
        mAllocatedSize = 0;
    }

    std::map<StringView, StringView> mInner;

private:
    size_t mAllocatedSize = 0;
};


template <typename T>
class SizedVector {
public:
    void PushBack(T val) { mInner.push_back(val); }

    size_t DataSize() const { return sizeof(decltype(mInner)) + mAllocatedSize; }

    void Clear() {
        mInner.clear();
        mAllocatedSize = 0;
    }

    std::vector<T> mInner;

private:
    size_t mAllocatedSize = 0;
};

template <>
class SizedVector<std::pair<StringView, StringView>> {
public:
    void Insert(StringView key, StringView val) {
        auto iter = std::find_if(mInner.begin(), mInner.end(), [key](const auto& item) { return item.first == key; });
        if (iter != mInner.end()) {
            mAllocatedSize += val.size() - iter->second.size();
            iter->second = val;
        } else {
            mAllocatedSize += key.size() + val.size();
            mInner.emplace_back(key, val);
        }
    }

    void PushBack(StringView key, StringView val) {
        mInner.emplace_back(key, val);
        mAllocatedSize += key.size() + val.size();
    }

    void SetNameByIndex(size_t index, StringView newKey) {
        if (index < mInner.size()) {
            mAllocatedSize = mAllocatedSize - mInner[index].first.size() + newKey.size();
            mInner[index].first = newKey;
        }
    }

    void Erase(StringView key) {
        auto iter = std::find_if(mInner.begin(), mInner.end(), [key](const auto& item) { return item.first == key; });
        if (iter != mInner.end()) {
            mAllocatedSize -= (iter->first.size() + iter->second.size());
            mInner.erase(iter);
        }
    }

    void FinalizeItems(const std::function<bool(std::pair<StringView, StringView>)>& isValid) {
        size_t index = 0;
        mAllocatedSize = 0;
        for (const auto& item : mInner) {
            if (!isValid(item)) {
                continue;
            }
            mInner[index++] = item;
            mAllocatedSize += item.first.size() + item.second.size();
        }
        mInner.resize(index);
    }

    size_t DataSize() const { return sizeof(decltype(mInner)) + mAllocatedSize; }

    void Clear() {
        mInner.clear();
        mAllocatedSize = 0;
    }

    std::vector<std::pair<StringView, StringView>> mInner;

private:
    size_t mAllocatedSize = 0;
};

} // namespace logtail
