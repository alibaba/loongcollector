//
// Created by qianlu on 2024/6/19.
//

#pragma once

#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

extern "C" {
#include <coolbpf/net.h>
}

namespace logtail {
namespace ebpf {

using PerfBufferSampleHandler = void (*)(void *ctx, int cpu, void *data, uint32_t size);
using PerfBufferLostHandler = void (*)(void *ctx, int cpu, unsigned long long cnt);
typedef int (*eBPFLogHandler)(int16_t level, const char *format, va_list args);

struct ObserverNetworkOption {
    std::vector<std::string> mEnableProtocols;
    bool mDisableProtocolParse = false;
    bool mDisableConnStats = false;
    bool mEnableConnTrackerDump = false;
    bool mEnableSpan = false;
    bool mEnableMetric = false;
    bool mEnableLog = false;
    bool mEnableCidFilter = false;
    std::vector<std::string> mEnableCids;
    std::vector<std::string> mDisableCids;
    std::string mMeterHandlerType;
    std::string mSpanHandlerType;
};

struct PerfBufferSpec {
public:
  PerfBufferSpec(const std::string& name, ssize_t size, void* ctx, PerfBufferSampleHandler scb, PerfBufferLostHandler lcb):
    mName(name), mSize(size), mCtx(ctx), mSampleHandler(scb), mLostHandler(lcb) {}
  std::string mName;
  ssize_t mSize = 0;
  void* mCtx;
  PerfBufferSampleHandler mSampleHandler;
  PerfBufferLostHandler mLostHandler;
};


enum class PluginType {
    NETWORK_OBSERVE,
    PROCESS_OBSERVE,
    FILE_OBSERVE,
    PROCESS_SECURITY,
    FILE_SECURITY,
    NETWORK_SECURITY,
    MAX,
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
        return mDestAddrList == other.mDestAddrList && mDestPortList == other.mDestPortList
            && mDestAddrBlackList == other.mDestAddrBlackList && mDestPortBlackList == other.mDestPortBlackList
            && mSourceAddrList == other.mSourceAddrList && mSourcePortList == other.mSourcePortList
            && mSourceAddrBlackList == other.mSourceAddrBlackList && mSourcePortBlackList == other.mSourcePortBlackList;
    }
};

struct SecurityOption {
    std::vector<std::string> call_names_;
    std::variant<std::monostate, SecurityFileFilter, SecurityNetworkFilter> filter_;
    bool operator==(const SecurityOption& other) const {
        return call_names_ == other.call_names_ && filter_ == other.filter_;
    }
};

struct NetworkObserveConfig {
    std::string mBtf;
    std::string mSo;
    long mUprobeOffset;
    long mUpcaOffset;
    long mUppsOffset;
    long mUpcrOffset;

    void* mCustomCtx;
    eBPFLogHandler mLogHandler;

    // perfworkers ...
    net_ctrl_process_func_t mCtrlHandler = nullptr;
    net_data_process_func_t mDataHandler = nullptr;
    net_statistics_process_func_t mStatsHandler = nullptr;
    net_lost_func_t mLostHandler = nullptr;

    bool mEnableCidFilter = false;
    int mCidOffset = -1;

    std::vector<std::string> mEnableContainerIds;
    std::vector<std::string> mDisableContainerIds;
};

struct ProcessConfig {
    std::vector<SecurityOption> options_;
    std::vector<PerfBufferSpec> mPerfBufferSpec;
    bool operator==(const ProcessConfig& other) const {
        return options_ == other.options_;
    }
};

struct NetworkSecurityConfig {
    std::vector<SecurityOption> options_;
    std::vector<PerfBufferSpec> mPerfBufferSpec;
    bool operator==(const NetworkSecurityConfig& other) const { return options_ == other.options_; }
};

struct FileSecurityConfig {
    std::vector<SecurityOption> options_;
    std::vector<PerfBufferSpec> mPerfBufferSpec;
    bool operator==(const FileSecurityConfig& other) const { return options_ == other.options_; }
};

enum class eBPFLogType
{
	NAMI_LOG_TYPE_WARN = 0,
	NAMI_LOG_TYPE_INFO,
	NAMI_LOG_TYPE_DEBUG,
};

struct PluginConfig {
    PluginType mPluginType;
    // log control
    std::variant<NetworkObserveConfig, ProcessConfig, NetworkSecurityConfig, FileSecurityConfig> mConfig;
};


/// to be deleted ...

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
  AbstractSecurityEvent(std::vector<std::pair<std::string, std::string>>&& tags, uint64_t ts)
    : tags_(tags), timestamp_(ts) {}
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
  std::string app_name_;
  std::string ip_;
  std::string host_;
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
  std::string app_name_;
  std::string host_ip_;
  std::string host_name_;
  std::vector<std::unique_ptr<SingleSpan>> single_spans_;
};

class SingleEvent {
public:
  explicit SingleEvent(){}
  explicit SingleEvent(std::vector<std::pair<std::string, std::string>>&& tags, uint64_t ts)
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
  explicit ApplicationBatchEvent(){}
  explicit ApplicationBatchEvent(const std::string& app_id, std::vector<std::pair<std::string, std::string>>&& tags) : app_id_(app_id), tags_(tags) {}
  explicit ApplicationBatchEvent(const std::string& app_id, std::vector<std::pair<std::string, std::string>>&& tags, std::vector<std::unique_ptr<SingleEvent>>&& events) 
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


}

}
