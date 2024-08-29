//
// Created by qianlu on 2024/6/19.
//

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <variant>

enum class SecureEventType {
  SECURE_EVENT_TYPE_SOCKET_SECURE,
  SECURE_EVENT_TYPE_FILE_SECURE,
  SECURE_EVENT_TYPE_PROCESS_SECURE,
  SECURE_EVENT_TYPE_MAX,
};

class AbstractSecurityEvent {
public:
  AbstractSecurityEvent(std::vector<std::pair<std::string, std::string>>&& tags, SecureEventType type, uint64_t ts)
    : tags_(tags), type_(type), timestamp_(ts) {}
  SecureEventType GetEventType() {return type_;}
  std::vector<std::pair<std::string, std::string>> GetAllTags() { return tags_; }
  uint64_t GetTimestamp() { return timestamp_; }
  void SetEventType(SecureEventType type) { type_ = type; }
  void SetTimestamp(uint64_t ts) { timestamp_ = ts; }
  void AppendTags(std::pair<std::string, std::string>&& tag) {
    tags_.emplace_back(std::move(tag));
  }

private:
  std::vector<std::pair<std::string, std::string>> tags_;
  SecureEventType type_;
  uint64_t timestamp_;
};

class BatchAbstractSecurityEvent {
public:
  BatchAbstractSecurityEvent(){}
  std::vector<std::pair<std::string, std::string>> GetAllTags() { return tags_; }
  uint64_t GetTimestamp() { return timestamp_; }
private:
  std::vector<std::pair<std::string, std::string>> tags_;
  uint64_t timestamp_;
  std::vector<std::unique_ptr<AbstractSecurityEvent>> events;
};

using HandleSingleDataEventFn = std::function<void(std::unique_ptr<AbstractSecurityEvent>&& event)>;
using HandleBatchDataEventFn = std::function<void(std::vector<std::unique_ptr<AbstractSecurityEvent>>&& events)>;

enum class UpdataType {
  SECURE_UPDATE_TYPE_ENABLE_PROBE,
  SECURE_UPDATE_TYPE_CONFIG_CHAGE,
  SECURE_UPDATE_TYPE_SUSPEND_PROBE,
  SECURE_UPDATE_TYPE_DISABLE_PROBE,
  SECURE_UPDATE_TYPE_MAX,
};


/////// sockettrace /////////


// Metrics Data
enum MeasureType {MEASURE_TYPE_APP, MEASURE_TYPE_NET, MEASURE_TYPE_PROCESS, MEASURE_TYPE_MAX};

struct AbstractSingleMeasure {
  virtual ~AbstractSingleMeasure() = default;
};

struct NetSingleMeasure : public AbstractSingleMeasure {
  uint64_t tcp_drop_total_;
  uint64_t tcp_retran_total_;
  uint64_t tcp_connect_total_;
  uint64_t tcp_rtt_;
  uint64_t tcp_rtt_var_;

  uint64_t recv_pkt_total_;
  uint64_t send_pkt_total_;
  uint64_t recv_byte_total_;
  uint64_t send_byte_total_;
};

struct AppSingleMeasure : public AbstractSingleMeasure {
  uint64_t request_total_;
  uint64_t slow_total_;
  uint64_t error_total_;
  uint64_t duration_ms_sum_;
  uint64_t status_2xx_count_;
  uint64_t status_3xx_count_;
  uint64_t status_4xx_count_;
  uint64_t status_5xx_count_;
};

struct Measure {
  // ip/rpc/rpc
  std::map<std::string, std::string> tags_;
  MeasureType type_;
  std::unique_ptr<AbstractSingleMeasure> inner_measure_;
};

// process
struct ApplicationBatchMeasure {
  std::string app_id_;
  std::string region_id_;
  std::string ip_;
  std::vector<std::unique_ptr<Measure>> measures_;
};

enum SpanKindInner { Unspecified, Internal, Server, Client, Producer, Consumer };
struct SingleSpan {
  std::map<std::string, std::string> tags_;
  std::string trace_id_;
  std::string span_id_;
  std::string span_name_;
  SpanKindInner span_kind_;

  uint64_t start_timestamp_;
  uint64_t end_timestamp_;
};

struct ApplicationBatchSpan {
  std::string app_id_;
  std::vector<std::unique_ptr<SingleSpan>> single_spans_;
};

class SingleEvent {
public:
  explicit __attribute__((visibility("default"))) SingleEvent(){}
  explicit __attribute__((visibility("default"))) SingleEvent(std::vector<std::pair<std::string, std::string>>&& tags, uint64_t ts)
    : tags_(tags), timestamp_(ts) {}
  std::vector<std::pair<std::string, std::string>> GetAllTags() { return tags_; }
  uint64_t GetTimestamp() { return timestamp_; }
  void SetTimestamp(uint64_t ts) { timestamp_ = ts; }
  void AppendTags(std::pair<std::string, std::string>&& tag) {
    tags_.emplace_back(std::move(tag));
  }

private:
  std::vector<std::pair<std::string, std::string>> tags_;
  uint64_t timestamp_;
};

class ApplicationBatchEvent {
public:
  explicit __attribute__((visibility("default"))) ApplicationBatchEvent(){}
  explicit __attribute__((visibility("default"))) ApplicationBatchEvent(const std::string& app_id, std::vector<std::pair<std::string, std::string>>&& tags) : app_id_(app_id), tags_(tags) {}
  explicit __attribute__((visibility("default"))) ApplicationBatchEvent(const std::string& app_id, std::vector<std::pair<std::string, std::string>>&& tags, std::vector<std::unique_ptr<SingleEvent>>&& events) 
    : app_id_(app_id), tags_(std::move(tags)), events_(std::move(events)) {}
  void SetEvents(std::vector<std::unique_ptr<SingleEvent>>&& events) { events_ = std::move(events); }
  void AppendEvent(std::unique_ptr<SingleEvent>&& event) { events_.emplace_back(std::move(event)); }
  void AppendEvents(std::vector<std::unique_ptr<SingleEvent>>&& events) { 
    for (auto& x : events) {
      events_.emplace_back(std::move(x));
    }
  }
  std::string app_id_; // pid
  std::vector<std::pair<std::string, std::string>> tags_; // container.id
  std::vector<std::unique_ptr<SingleEvent>> events_;
};

/////// merged config /////////

namespace nami {

enum class PluginType {
  NETWORK_OBSERVE,
  PROCESS_OBSERVE,
  FILE_OBSERVE,
  PROCESS_SECURITY,
  FILE_SECURITY,
  NETWORK_SECURITY,
  MAX,
};

// observe metrics
using NamiHandleBatchMeasureFunc = std::function<void(std::vector<std::unique_ptr<ApplicationBatchMeasure>>&& measures, uint64_t timestamp)>;
// observe spans
using NamiHandleBatchSpanFunc = std::function<void(std::vector<std::unique_ptr<ApplicationBatchSpan>>&&)>;
// observe events
using NamiHandleBatchEventFunc = std::function<void(std::vector<std::unique_ptr<ApplicationBatchEvent>>&&)>;
// observe security
using NamiHandleBatchDataEventFn = std::function<void(std::vector<std::unique_ptr<AbstractSecurityEvent>>&& events)>;

struct ObserverNetworkOption {
    std::vector<std::string> mEnableProtocols;
    bool mDisableProtocolParse = false;
    bool mDisableConnStats = false;
    bool mEnableConnTrackerDump = false;
    bool mEnableSpan = false;
    bool mEnableMetric = false;
    bool mEnableLog = true;
    std::string mMeterHandlerType;
    std::string mSpanHandlerType;
};

// file
struct SecurityFileFilter {
    std::vector<std::string> mFilePathList;
    bool operator==(const SecurityFileFilter& other) const { return mFilePathList == other.mFilePathList; }
};

// network
struct SecurityNetworkFilter {
    std::vector<std::string> mDestAddrList;
    std::vector<uint32_t> mDestPortList;
    std::vector<std::string> mDestAddrBlackList;
    std::vector<uint32_t> mDestPortBlackList;
    std::vector<std::string> mSourceAddrList;
    std::vector<uint32_t> mSourcePortList;
    std::vector<std::string> mSourceAddrBlackList;
    std::vector<uint32_t> mSourcePortBlackList;
    bool operator==(const SecurityNetworkFilter& other) const {
      return mDestAddrList == other.mDestAddrList &&
             mDestPortList == other.mDestPortList &&
             mDestAddrBlackList == other.mDestAddrBlackList &&
             mDestPortBlackList == other.mDestPortBlackList &&
             mSourceAddrList == other.mSourceAddrList &&
             mSourcePortList == other.mSourcePortList &&
             mSourceAddrBlackList == other.mSourceAddrBlackList &&
            mSourcePortBlackList == other.mSourcePortBlackList;
    }
};

struct SecurityOption {
  std::vector<std::string> call_names_;
  std::variant<std::monostate, SecurityFileFilter, SecurityNetworkFilter> filter_;
  bool operator==(const SecurityOption& other) const {
    return call_names_ == other.call_names_ &&
            filter_ == other.filter_;
  }
};

struct NetworkObserveConfig {
  bool enable_libbpf_debug_ = false;
  bool enable_so_ = false;
  std::string btf_;
  int32_t btf_size;
  std::string so_;
  int32_t so_size_;
  long uprobe_offset_;
  long upca_offset_;
  long upps_offset_;
  long upcr_offset_;
  bool enable_span_ = false;
  bool enable_metric_ = false;
  bool enable_event_ = false;
  NamiHandleBatchMeasureFunc measure_cb_ = nullptr;
  NamiHandleBatchSpanFunc span_cb_ = nullptr;
  NamiHandleBatchEventFunc event_cb_ = nullptr;
  bool operator==(const NetworkObserveConfig& other) const {
    return enable_libbpf_debug_ == other.enable_libbpf_debug_ &&
           enable_so_ == other.enable_so_ &&
           btf_ == other.btf_ &&
           btf_size == other.btf_size &&
           so_ == other.so_ &&
           so_size_ == other.so_size_ &&
           uprobe_offset_ == other.uprobe_offset_ &&
           upca_offset_ == other.upca_offset_ &&
           upps_offset_ == other.upps_offset_ &&
           upcr_offset_ == other.upcr_offset_;
  }
};

struct ProcessConfig {
  
  bool enable_libbpf_debug_ = false;

  std::vector<SecurityOption> options_;
  NamiHandleBatchDataEventFn process_security_cb_;
  bool operator==(const ProcessConfig& other) const {
    return enable_libbpf_debug_ == other.enable_libbpf_debug_ &&
           options_ == other.options_;
  }
};

struct NetworkSecurityConfig {
  std::vector<SecurityOption> options_;
  NamiHandleBatchDataEventFn network_security_cb_;
  bool operator==(const NetworkSecurityConfig& other) const {
    return options_ == other.options_;  
  }
};

struct FileSecurityConfig {
  std::vector<SecurityOption> options_;
  NamiHandleBatchDataEventFn file_security_cb_;
  bool operator==(const FileSecurityConfig& other) const {
    return options_ == other.options_;  
  }
};
struct eBPFConfig {
  PluginType plugin_type_;
  UpdataType type = UpdataType::SECURE_UPDATE_TYPE_ENABLE_PROBE;
  // common config
  std::string host_name_;
  std::string host_ip_;
  std::string host_path_prefix_;
  // specific config
  std::variant<NetworkObserveConfig, ProcessConfig, NetworkSecurityConfig, FileSecurityConfig> config_;
  bool operator==(const eBPFConfig& other) const {
    return plugin_type_ == other.plugin_type_ &&
           type == other.type &&
           host_name_ == other.host_name_ &&
           host_ip_ == other.host_ip_ &&
           host_path_prefix_ == other.host_path_prefix_ &&
           config_ == other.config_;
  }
};

};
