//
// Created by qianlu on 2024/5/20.
//

#pragma once

#include <mutex>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "common/Lock.h"
#include "ebpf/type/NetworkObserverEvent.h"
#include "ebpf/type/table/AppTable.h"
#include "metadata/ContainerInfo.h"

extern "C" {
#include <net.h>
};

namespace logtail {
namespace ebpf {

class ConnTracker {
public:
    ~ConnTracker() {
        protocol_ = ProtocolType::UNKNOWN;
        aggregate_key_ = "";
        epoch_ = 0;
        // ....
    }
    ConnTracker(const ConnId& conn_id)
        : protocol_(ProtocolType::UNKNOWN),
          aggregate_key_(""),
          net_meta_attached_(false),
          k8s_meta_attached_(false),
          k8s_peer_meta_attached_(false),
          protocol_set_(false),
          epoch_(5),
          close_(false),
          last_update_timestamp(0),
          last_active_timestamp(INT64_MAX),
          conn_id_(conn_id),
          role(support_role_e::IsUnknown),
          curr_conn_stats_record_(std::make_shared<ConnStatsRecord>(conn_id)),
          last_conn_stats_record_(std::make_shared<ConnStatsRecord>(conn_id)),
          diff_conn_stats_record_(std::make_shared<ConnStatsRecord>(conn_id)) {
        // ::memset(&addr, 0, sizeof(union sockaddr_t));
        // ::memset(&si, 0, sizeof(struct socket_info));
    }
    void UpdateConnStats(struct conn_stats_event_t* event);
    void UpdateConnState(struct conn_ctrl_event_t* event);
    void SafeUpdateProtocol(ProtocolType protocol);
    void SafeUpdateRole(enum support_role_e role);

    // TODO @qianlu.kk
    std::unique_ptr<AbstractRecord> GetConnStatsGauge();
    // KeyValue GetCommonAttr();
    std::array<std::string, kConnTrackerElementsTableSize> GetConnTrackerAttrs();

    const ConnId GetConnId() const {
        ReadLock lock(mReadWriteLock);
        return conn_id_;
    };

    bool ReadyToDestroy(const std::chrono::time_point<std::chrono::steady_clock>& now) {
        ReadLock lock(mReadWriteLock);
        if (close_ && this->epoch_ < 0) {
            return true;
        }
        auto now_ts = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        return now_ts > last_active_timestamp && (now_ts - last_active_timestamp) > 120000; // 120s
    }

    bool IsClose() const { return close_; }

    std::shared_ptr<ConnStatsRecord> GenerateConnStatsRecord() const;

    int GetEpoch() const { return epoch_; }

    void CountDownLatch() { this->epoch_--; }

    uint64_t GetLastUpdateTs() const {
        ReadLock lock(mReadWriteLock);
        return last_update_timestamp;
    }
    uint64_t GetLastActiveTs() const {
        ReadLock lock(mReadWriteLock);
        return last_active_timestamp;
    }

    bool MetaAttachReadyForApp();
    bool MetaAttachReadyForNet();

    void RecordActive() {
        WriteLock lock(mReadWriteLock);
        this->epoch_ = 5;
        auto now = std::chrono::steady_clock::now();
        last_active_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    }

    void UpdateSelfPodMeta(const std::shared_ptr<k8sContainerInfo>& pod);
    void MarkPodMetaAttached() { k8s_meta_attached_ = true; }
    void UpdatePeerPodMeta(const std::shared_ptr<k8sContainerInfo>& pod);
    void MarkPeerPodMetaAttached() { k8s_peer_meta_attached_ = true; }
    void UpdatePeerPodMetaForExternal();
    void UpdatePeerPodMetaForLocalhost();
    void UpdateSelfPodMetaForUnknown();

    std::shared_ptr<ConnStatsRecord> GetConnStatsRecord() const;

    void UpdateReportTs(int64_t ts) { last_report_conn_stats_ts_ = ts; }

    int64_t GetLastReportTs() const { return last_report_conn_stats_ts_; }

    std::string GetSourceIp();
    std::string GetRemoteIp();
    std::string GetSourceAddr();
    std::string GetRemoteAddr();

    mutable ReadWriteLock mReadWriteLock;

    ProtocolType protocol_;
    std::string aggregate_key_;
    volatile bool net_meta_attached_ = false;
    volatile bool k8s_meta_attached_ = false;
    volatile bool k8s_peer_meta_attached_ = false;
    volatile bool protocol_set_ = false;
    std::string container_id_;
    std::string container_id_trim_;

    std::string saddr_;
    std::string daddr_;
    std::string sip_;
    std::string dip_;
    uint32_t sport_;
    uint32_t dport_;
    int net_ns_;
    std::string family_;

private:
    void UpdateSocketInfo(struct conn_stats_event_t* event);
    // void InitCommonAttrs();
    void InitEventCommonAttrs();
    void UpdateProtocol(ProtocolType protocol);
    void UpdateRole(enum support_role_e role);

    void MarkClose() {
        this->close_ = true;
        this->mark_close_time_ = std::chrono::steady_clock::now();
    }

    void RecordLastUpdateTs(uint64_t ts) { last_update_timestamp = ts; }

    enum class EventCommonAttrKeyTab {
        FD,
        START,
        PID,
        CONTAINER_ID,
        REMOTE_IP,
        REMOTE_PORT,
        SOURCE_IP,
        SOURCE_PORT,
        FAMILY,
        PROTOCOL,
        RPC_TYPE,
        CALL_KIND,
        CALL_TYPE,
        K8S_POD_NAME,
        K8S_POD_IP,
        K8S_NAMESPACE,
        K8S_WORKLOAD_NAME,
        K8S_WORKLOAD_KIND,
        PEER_K8S_POD_NAME,
        PEER_K8S_POD_IP,
        PEER_K8S_NAMESPACE,
        PEER_K8S_WORKLOAD_NAME,
        PEER_K8S_WORKLOAD_KIND,
        PEER_K8S_SERVICE_NAME,
        ARMS_APP_NAME,
        PEER_ARMS_APP_NAME,
        NET_NS,
        SOURCE_IP_ORIGIN,
        REMOTE_IP_ORIGIN,
        MAX,
    };
    std::array<std::string, kConnTrackerElementsTableSize> attrs_;
    volatile int epoch_;
    volatile bool close_;
    int64_t last_update_timestamp;
    int64_t last_active_timestamp;
    ConnId conn_id_;
    enum support_role_e role;

    std::chrono::time_point<std::chrono::steady_clock> mark_close_time_;

    std::string pod_ip_;
    std::string pod_name_;
    std::string namespace_;
    std::string workload_name_;
    std::string workload_kind_;
    std::string peer_pod_ip_;
    std::string peer_pod_name_;
    std::string peer_namespace_;
    std::string peer_workload_name_;
    std::string peer_workload_kind_;
    std::string peer_service_name_;
    std::string arms_app_name_;
    std::string arms_app_id_;
    std::string peer_arms_app_name_;

    // conn tracker attrs

    uint32_t saddr_ori_;
    uint32_t daddr_ori_;
    // struct socket_info si;


    static std::regex rgx_;

    std::shared_ptr<ConnStatsRecord> curr_conn_stats_record_;
    std::shared_ptr<ConnStatsRecord> last_conn_stats_record_;
    mutable std::shared_ptr<ConnStatsRecord> diff_conn_stats_record_;
    int64_t last_report_conn_stats_ts_ = INT64_MIN;
};


} // namespace ebpf
} // namespace logtail
