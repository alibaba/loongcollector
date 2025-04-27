// Copyright 2025 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <mutex>
#include <queue>

#include "BPFMapTraits.h"
#include "Log.h"

namespace logtail {
namespace ebpf {

#define ERR_LIMIT_EXCEEDED -1

template <typename BPFMap>
class IdManager {
public:
    IdManager() : mIdMax(logtail::ebpf::BPFMapTraits<BPFMap>::outter_max_entries) {}
    int GetMaxId() { return mIdMax; }
    int GetNextId() {
        std::lock_guard<std::mutex> lock(mMtx);
        if (!mReleasedIds.empty()) {
            int id = mReleasedIds.front();
            mReleasedIds.pop();
            return id;
        }

        if (mNextId >= mIdMax) {
            return ERR_LIMIT_EXCEEDED;
        }
        return mNextId++;
    }
    void ReleaseId(int id) {
        std::lock_guard<std::mutex> lock(mMtx);

        if (id < 0 || id >= mIdMax) {
            return;
        }

        mReleasedIds.push(id);
    }

private:
    int mIdMax;
    int mNextId = 0;
    std::queue<int> mReleasedIds;
    std::mutex mMtx;
};

class IdAllocator {
public:
    static IdAllocator* GetInstance() {
        static IdAllocator instance;
        return &instance;
    }

    template <typename BPFMap>
    int GetNextId() {
        return GetIdManager<BPFMap>().GetNextId();
    }

    template <typename BPFMap>
    void ReleaseId(int id) {
        return GetIdManager<BPFMap>().ReleaseId(id);
    }

    template <typename BPFMap>
    int GetMaxId() {
        return GetIdManager<BPFMap>().GetMaxId();
    }

private:
    IdAllocator() {}

    template <typename BPFMap>
    IdManager<BPFMap>& GetIdManager() {
        static IdManager<BPFMap> sManager;
        return sManager;
    }
};

} // namespace ebpf
} // namespace logtail
