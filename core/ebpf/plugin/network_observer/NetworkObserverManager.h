//
// Created by qianlu on 2024/7/1.
//

#pragma once

extern "C" {
#include <coolbpf.h>
#include <net.h>
}
#include <stdio.h>
#include <arpa/inet.h>
#include <iostream>
#include <stdbool.h>
#include <coolbpf.h>
#include <memory>
#include <unordered_map>
#include <atomic>
#include <vector>

#include "ebpf/plugin/AbstractManager.h"
#include "common/queue/blockingconcurrentqueue.h"

// #include "export/export.h"
// #include "conn_tracker_manager.h"
// #include "util/queue/concurrentqueue.h"
// #include "type/abstract_raw_event.h"
// #include "type/ebpf_type.h"
// #include "type/type.h"
// #include "workers.h"
// #include "type/span_data.h"
// #include "type/metric_data.h"
// #include "type/abstract_record.h"
// #include "workers.h"

// #include "src/bpf_wrapper.h"
// #include "type/tables/app_table.h"
// #include "type/tables/net_table.h"
// #include "type/tables/converge_table.h"
// #include "common/convergence/sdk/ConvergerSDK.h"
// #include "common/sampler/SamplerSDK.h"


namespace logtail {
namespace ebpf {

// template class WorkerFunc<std::unique_ptr<NetDataEvent>, std::unique_ptr<AbstractRecord>>;

// class SocketTrace : public AbstractManager {
// public:
//   static std::shared_ptr<SocketTrace> Create(std::unique_ptr<BaseManager>& mgr) {
//     return std::make_shared<SocketTrace>(mgr);
//   }
//   SocketTrace(std::unique_ptr<BaseManager>& mgr) : AbstractManager(mgr) {}
//   // Step1. init ebpf module and register callbacks
//   // Step2. start a thread to parse the
//   int Init(std::shared_ptr<nami::eBPFConfig>) override;
//   int Destroy() override;
//   int EnableCallName(const std::string& call_name, const configType config) override { return 0; }
//   int DisableCallName(const std::string& call_name) override { return 0; }
// //   virtual PluginType GetPluginType() override { return PluginType::NETWORK_OBSERVE; }
//   void UpdateWhitelists(std::vector<std::string>&& enableCids, std::vector<std::string>&& disableCids);
//   }

//   void Stop();

//   static int HandlerPrint(short int level, const char *format, va_list args);
//   static void ProcessNetCtrlEventHandler(void *custom_data, struct conn_ctrl_event_t *event_data);
//   static void ProcessNetDataEventHandler(void *custom_data, struct conn_data_event_t *event_data);
//   static void ProcessNetStatsEventHandler(void *custom_data, struct conn_stats_event_t *event_data);
//   static void ProcessNetLostEventhandler(void *custom_data, enum callback_type_e type, uint64_t lost_count);

//   void RecordEventLost(enum callback_type_e type, uint64_t lost_count);

//   void AcceptNetCtrlEvent(struct conn_ctrl_event_t* event);
//   void AcceptNetStatsEvent(struct conn_stats_event_t *event);

//   void EnqueueDataEvent(std::unique_ptr<NetDataEvent> data_event) const {
//     recv_http_data_events_total_.fetch_add(1);
//     auto ct = conntracker_mgr_->GetOrCreateConntracker(data_event->conn_id);
//     if (ct) {
//       ct->RecordActive();
//       ct->SafeUpdateRole(data_event->role);
//       ct->SafeUpdateProtocol(data_event->protocol);
//     } else {
//       data_events_drop_total_.fetch_add(1);
//       LOG(WARNING) << "cannot find or create conn_tracker, skip data event ... ";
//     }
//     raw_event_queue_.enqueue(std::move(data_event));
//   }

//   void PollBufferWrapper();

//   // Register Span Callback ...
//   // Register Metrics Callback ...
// private:
//   void ConsumeRecords();
//   std::string Record2FileLog(const std::shared_ptr<HttpRecord> &);
//   void ConsumeRecordsAsFileLogs(const std::vector<std::shared_ptr<AbstractRecord>> &records, size_t count);
//   void ConsumeRecordsAsEvent(const std::vector<std::shared_ptr<AbstractRecord>> &records, size_t count);
  
//   void SetCoolBpfConfig(int32_t opt, int32_t value) {
//     int32_t *params[] = {&value};
//     int32_t paramsLen[] = {4};
//     ebpf_config(opt, 0, 1, (void **)params, paramsLen);
//   }

//   void RunInThread();

//   std::shared_ptr<ConnTrackerManager> conntracker_mgr_;

//   // TODO @qianlu.kk modify T for abstract event
//   // store raw events
//   mutable moodycamel::BlockingConcurrentQueue<std::unique_ptr<NetDataEvent>> raw_event_queue_;

//   mutable std::atomic_long data_events_drop_total_ = 0;

//   mutable std::atomic_int64_t conntracker_num_ = 0;
//   mutable std::atomic_int64_t recv_conn_stat_events_total_ = 0;
//   mutable std::atomic_int64_t recv_ctrl_events_total_ = 0;
//   mutable std::atomic_int64_t recv_http_data_events_total_ = 0;
//   mutable std::atomic_int64_t lost_conn_stat_events_total_ = 0;
//   mutable std::atomic_int64_t lost_ctrl_events_total_ = 0;
//   mutable std::atomic_int64_t lost_data_events_total_ = 0;

//   mutable std::atomic_int64_t parse_http_records_success_total_ = 0;
//   mutable std::atomic_int64_t parse_http_records_failed_total_ = 0;
//   mutable std::atomic_int64_t agg_map_entities_num_ = 0;

//   int consume_wait_ms_ = 200;
//   int consume_batch_max_ = 4096;
//   int metric_consume_wait_ms_ = 200;
//   int metric_consume_batch_max_ = 4096;
//   bool is_prod = false;
//   double sample_rate_ = 1.0;
//   SamplerSDK sampler_;

//   // store parsed records
//   moodycamel::BlockingConcurrentQueue<std::shared_ptr<AbstractRecord>> record_queue_;
  
//   // WorkerPool: used to parse protocols from raw record
//   // NetDataHandler netDataHandler_;
//   std::unique_ptr<WorkerPool<std::unique_ptr<NetDataEvent>, std::shared_ptr<AbstractRecord>>> worker_pool_;

//   std::thread core_thread_;
//   std::thread record_consumer_thread_;

//   // std::atomic_bool tflag_;

//   // callback functions for oneagent
//   nami::NamiHandleBatchSpanFunc span_cb_;
//   std::shared_mutex span_cb_mutex_;
//   nami::NamiHandleBatchEventFunc event_cb_;
//   std::shared_mutex event_cb_mutex_;
//   ConnIdSearchCallbackFn sock_meta_cb_;
//   std::shared_mutex sock_meta_cb_mutex_;

//   std::atomic_bool enable_span_;
//   std::atomic_bool enable_event_;
//   std::atomic_bool enable_metrics_;

//   FrequencyManager sockettrace_consumer_freq_mgr_;
//   FrequencyManager poll_buffer_freq_mgr_;

//   // TODO @qianlu.kk 
//   std::unique_ptr<Aggregator> metric_aggregator_ = nullptr;
// };


}
}


// TODO @qianlu.kk for converger ...

// template <typename T>
// struct MetricsTableSelector;

// template <>
// struct MetricsTableSelector<AbstractAppRecord> {
//     static constexpr const nami::DataTableSchema& GetTableSchema() { return nami::kAppMetricsTable; }
// };

// template <>
// struct MetricsTableSelector<ConnStatsRecord> {
//     static constexpr const nami::DataTableSchema& GetTableSchema() { return nami::kNetMetricsTable; }
// };

// template <typename T>
// void ConvergeAbstractRecord(const std::shared_ptr<T>& record) {
//     auto table = MetricsTableSelector<T>::GetTableSchema();
//     for (size_t i = 0; i < record->metric_attributes_.size(); i++) {
//         auto name = table.ColName(i);
//         if (nami::MetricsConvergeConfigMap.count(name) > 0) {
//             auto origin_value = record->metric_attributes_[i];
//             auto converged_value = ConvergerSDK::GetInstance().converge(
//                 nami::MetricsConvergeConfigMap.find(name)->second, origin_value);
//             if (converged_value.size() == 0 && origin_value.size() > 0) {
//                 LOG(WARNING) << "converge sdk returns empty value, which should "
//                                 "not happen, original_value: "
//                              << origin_value;
//                 converged_value = origin_value;
//             }
//             if (converged_value != origin_value) {
//                 // todo: @anji update span attributes
//                 LOG(WARNING) << "converge sdk working, origin:"
//                              << origin_value << " after:" << converged_value;
//                 record->metric_attributes_[i] = converged_value;
//             } else {
//               // LOG(INFO) << "no need to converge for " << converged_value;
//             }
//         }
//     }
// }
