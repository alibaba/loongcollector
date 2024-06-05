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

} // namespace logtail
