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

// #include "src/base_manager.h"
#include "ebpf/include/export.h"
// #include "driver/bpf_wrapper.h"
// #include "common/agg_tree.h"
// #include "type/security_event.h"

namespace logtail {
namespace ebpf {

class AbstractManager {
// public:
//     static std::atomic_int global_idx_;
//     using configType = std::variant<std::monostate, nami::SecurityFileFilter,
//                      nami::SecurityNetworkFilter>;
//     AbstractManager(std::unique_ptr<BaseManager>&);
//     virtual ~AbstractManager();
//     virtual int Init(std::shared_ptr<nami::eBPFConfig>) = 0;
//     // deprecated
//     virtual int Attach(std::vector<AttachProgOps> &attach_ops) = 0;
//     // deprecated
//     virtual int Detach(std::vector<AttachProgOps> &attach_ops) = 0;
//     virtual int Destroy() = 0;
//     // deprecated
//     virtual int SetTailCall(const std::string &map_name,
//                             const std::vector<std::string> &functions) = 0;
//     // deprecated
//     // handle parameter variable change
//     virtual int UpdateConfig(const std::shared_ptr<nami::eBPFConfig> &new_config) = 0;

//     virtual int EnableCallName(const std::string& call_name, const configType config) = 0;

//     bool IsRunning();

//     virtual nami::PluginType GetPluginType() = 0;

//     virtual void InitAggregateTree() {
//         aggregate_tree_ = nullptr;
//     }

//     virtual void Suspend() {
//         std::lock_guard<std::mutex> lk(mtx_);
//         // flag_ = false;
//         suspend_flag_ = true;
//         return;
//     }

//     virtual void Resume() {
//         std::lock_guard<std::mutex> lk(mtx_);
//         suspend_flag_ = false;
//         return;
//     }
    
//     virtual int DisableCallName(const std::string& call_name) = 0;
//     virtual std::shared_ptr<eBPFStatistics> GetStatistics();
//     void UpdateRecvKernelEventsTotal(int n = 1) { recv_kernel_events_total_.fetch_add(n); }
//     void UpdateLossKernelEventsTotal(int n = 1) { loss_kernel_events_total_.fetch_add(n); }
//     void UpdatePushSpanTotal(int n = 1) { push_spans_total_.fetch_add(n); }
//     void UpdatePushMetricTotal(int n = 1) { push_metrics_total_.fetch_add(n); }
//     void UpdatePushEventTotal(int n = 1) { push_events_total_.fetch_add(n); }

//     std::mutex mtx_;
//     std::atomic<bool> flag_ = false;
//     std::atomic<bool> suspend_flag_ = false;
// protected:
//     int InitOrGetCallNameIdx(const std::string& call_name);
//     int ReleaseCallNameIdx(const std::string& call_name);
//     int GetCallNameIdx(const std::string& call_name);

//     void UpdateStatisticsInner();

//     virtual std::shared_ptr<eBPFStatistics> AllocNewStats() {
//         return std::make_shared<eBPFStatistics>();
//     }

//     // void UpdateRecvKernelEventsTotal(int n = 1) { process_cache_entities_num_.fetch_add(n); }
//     void UpdateProcessCacheMissTotal(int n = 1) { miss_process_cache_total_.fetch_add(n); }

//     void RecordIndex(enum secure_funcs call_name, int idx) {
//         call_name_indexes_[int(call_name)].insert(idx);
//     }
//     const std::set<int> GetCallNameAllIndex(enum secure_funcs call_name) const {
//         return call_name_indexes_[int(call_name)];
//     }
//     void RelaseCallNameIndexes(enum secure_funcs call_name) {
//         call_name_indexes_[int(call_name)].clear();
//     }
//     BaseManager *base_mgr_;
//     std::chrono::nanoseconds time_diff_;
//     // call name to idx
//     std::unordered_map<std::string, int> call_name_2_idx_;
//     std::array<std::set<int>, SECURE_FUNCS_MAX> call_name_indexes_;

//     std::queue<std::string> enable_callnames_;

//     std::unique_ptr<SIZETAggTree<BaseSecurityNode, std::unique_ptr<BaseSecurityEvent>>> aggregate_tree_;
//     uint64_t last_export_sec_ = 0;

//     std::atomic_int64_t recv_kernel_events_total_ = 0;
//     std::atomic_int64_t loss_kernel_events_total_ = 0;
//     std::atomic_int64_t push_spans_total_ = 0;
//     std::atomic_int64_t push_metrics_total_ = 0;
//     std::atomic_int64_t push_events_total_ = 0;
//     std::atomic_int64_t process_cache_entities_num_ = 0;
//     std::atomic_int64_t miss_process_cache_total_ = 0;

//     std::shared_ptr<eBPFStatistics> last_stats_;
//     std::shared_ptr<eBPFStatistics> curr_stats_;
//     std::shared_ptr<eBPFStatistics> diff_stats_;
};

}
}
