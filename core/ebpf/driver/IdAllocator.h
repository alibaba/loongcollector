#pragma once

#include <mutex>
#include <queue>

#include "BPFMapTraits.h"

namespace logtail {
namespace ebpf {

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
            return -1;
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
        static IdManager<BPFMap> manager;
        return manager;
    }
};

} // namespace ebpf
} // namespace logtail
