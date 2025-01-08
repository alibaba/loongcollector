// Copyright 2023 iLogtail Authors
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

#include <unordered_map>
#include <atomic>
#include <array>
#include <set>
#include <queue>

#include "ebpf/SourceManager.h"
#include "BaseManager.h"
#include "ebpf/Config.h"
#include "common/Lock.h"
#include "ebpf/include/export.h"
#include "ebpf/util/AggregateTree.h"
#include "ebpf/type/SecurityEvent.h"

// #include "driver/bpf_wrapper.h"
// #include "common/agg_tree.h"
// #include "type/security_event.h"

namespace logtail {
namespace ebpf {

class AbstractManager {
public:
    using configType = std::variant<std::monostate, logtail::ebpf::SecurityFileFilter,
                     logtail::ebpf::SecurityNetworkFilter>;
    AbstractManager() = delete;
    explicit AbstractManager(std::unique_ptr<BaseManager>&, std::shared_ptr<SourceManager> sourceManager);
    virtual ~AbstractManager() {}

    virtual int Init(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*> options) = 0;

    virtual int Destroy() = 0;

    virtual int EnableCallName(const std::string& call_name, const configType config) = 0;
    
    virtual int DisableCallName(const std::string& call_name) = 0;

    bool IsRunning();

    int GetCallNameIdx(const std::string& call_name);

    virtual logtail::ebpf::PluginType GetPluginType() = 0;

    // virtual void InitAggregateTree() {
    //     aggregate_tree_ = nullptr;
    // }

    virtual void Suspend() {
        WriteLock lock(mMtx);
        // flag_ = false;
        mSuspendFlag = true;
        return;
    }

    virtual void Resume() {
        WriteLock lock(mMtx);
        mSuspendFlag = false;
        return;
    }


    mutable ReadWriteLock mMtx;
    std::atomic<bool> mFlag = false;
    std::atomic<bool> mSuspendFlag = false;
protected:
    std::shared_ptr<SourceManager> mSourceManager;
    // int InitOrGetCallNameIdx(const std::string& call_name);
    // int ReleaseCallNameIdx(const std::string& call_name);
    // int GetCallNameIdx(const std::string& call_name);

    // void UpdateStatisticsInner();

    // virtual std::shared_ptr<eBPFStatistics> AllocNewStats() {
    //     return std::make_shared<eBPFStatistics>();
    // }

    // void UpdateProcessCacheMissTotal(int n = 1) { miss_process_cache_total_.fetch_add(n); }

    // void RecordIndex(enum secure_funcs call_name, int idx) {
    //     call_name_indexes_[int(call_name)].insert(idx);
    // }
    // const std::set<int> GetCallNameAllIndex(enum secure_funcs call_name) const {
    //     return call_name_indexes_[int(call_name)];
    // }
    // void RelaseCallNameIndexes(enum secure_funcs call_name) {
    //     call_name_indexes_[int(call_name)].clear();
    // }

    BaseManager *mBaseManager;
    std::chrono::nanoseconds mTimeDiff;
    // call name to idx
    // std::unordered_map<std::string, int> call_name_2_idx_;
    // std::array<std::set<int>, SECURE_FUNCS_MAX> call_name_indexes_;

    // std::queue<std::string> enable_callnames_;

    // std::unique_ptr<SIZETAggTree<BaseSecurityNode, std::unique_ptr<BaseSecurityEvent>>> aggregate_tree_;
    // uint64_t last_export_sec_ = 0;
};

}
}
