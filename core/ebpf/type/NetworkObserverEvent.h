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

#include <coolbpf/net.h>
#include <cstddef>

#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "ebpf/type/table/AppTable.h"
#include "ebpf/type/table/HttpTable.h"
#include "ebpf/type/table/NetTable.h"


namespace logtail {
namespace ebpf {

struct CaseInsensitiveLess {
    struct NoCaseCompare {
        bool operator()(const unsigned char c1, const unsigned char c2) const {
            return std::tolower(c1) < std::tolower(c2);
        }
    };

    template <typename TStringType>
    bool operator()(const TStringType& s1, const TStringType& s2) const {
        return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(), NoCaseCompare());
    }
};

using HeadersMap = std::multimap<std::string, std::string, CaseInsensitiveLess>;

enum class CallType {
    UNKNOWN,
    HTTP,
    HTTP_CLIENT,
    MYSQL,
    MYSQL_SERVER,
    DNS,
    DNS_SERVER,
    KAFKA_PRODUCER,
    KAFKA_CONSUMER,
    // others ...
};

enum class ProtocolType {
    UNKNOWN,
    HTTP,
    MYSQL,
    DNS,
    REDIS,
    KAFKA,
    PGSQL,
    MONGO,
    DUBBO,
    HSF,
    MAX,
};


enum class AggregateType {
    //  NET_L7,
    //  NET_L5,
    NETWORK,
    PROCESS,
    MAX,
};

enum class NetworkDataType {
    APP,
    L5,
    MAX,
};

enum class ProcessDataType {
    MAX,
};


inline ProtocolType& operator++(ProtocolType& pt) {
    pt = static_cast<ProtocolType>(static_cast<int>(pt) + 1);
    return pt;
}

inline ProtocolType operator++(ProtocolType& pt, int) {
    ProtocolType old = pt;
    pt = static_cast<ProtocolType>(static_cast<int>(pt) + 1);
    return old;
}

enum class EventType {
    UNKNOWN_EVENT = 0,
    CONN_STATS_EVENT = 1,
    HTTP_EVENT = 2,
    MYSQL_EVENT = 3,
    REDIS_EVENT = 4,
    DNS_EVENT = 5,
    PROCESS_EVENT = 6,
    MAX = 7,
};

enum class ConvergeType {
    IP,
    URL,
    PORT,
};


class ConnId {
public:
    int32_t fd;
    uint32_t tgid;
    uint64_t start;

    ConnId(int32_t fd, uint32_t tgid, uint64_t start) : fd(fd), tgid(tgid), start(start) {}

    ConnId(const ConnId& other) : fd(other.fd), tgid(other.tgid), start(other.start) {}
    ConnId& operator=(const ConnId& other) {
        if (this != &other) {
            fd = other.fd;
            tgid = other.tgid;
            start = other.start;
        }
        return *this;
    }

    ConnId(ConnId&& other) : fd(other.fd), tgid(other.tgid), start(other.start) {}
    ConnId& operator=(ConnId&& other) noexcept {
        if (this != &other) {
            fd = other.fd;
            tgid = other.tgid;
            start = other.start;
        }
        return *this;
    }

    explicit ConnId(const struct connect_id_t& conn_id) : fd(conn_id.fd), tgid(conn_id.tgid), start(conn_id.start) {}

    bool operator==(const ConnId& other) const { return fd == other.fd && tgid == other.tgid && start == other.start; }
};

struct ConnIdHash {
    inline static void combine(std::size_t& hash_result, std::size_t hash) {
        hash_result ^= hash + 0x9e3779b9 + (hash_result << 6) + (hash_result >> 2);
    }

    std::size_t operator()(const ConnId& obj) const {
        std::size_t hash_result = 0UL;
        combine(hash_result, std::hash<int32_t>{}(obj.fd));
        combine(hash_result, std::hash<uint32_t>{}(obj.tgid));
        combine(hash_result, std::hash<uint64_t>{}(obj.start));
        return hash_result;
    }
};

class NetDataEvent {
public:
    ConnId conn_id;
    uint64_t start_ts;
    uint64_t end_ts;
    ProtocolType protocol;
    enum support_role_e role;
    std::string req_msg;
    std::string resp_msg;

    explicit NetDataEvent(struct conn_data_event_t* conn_data)
        : conn_id(conn_data->conn_id),
          start_ts(conn_data->start_ts),
          end_ts(conn_data->end_ts),
          protocol(static_cast<ProtocolType>(conn_data->protocol)),
          role(conn_data->role) {
        req_msg = std::string(conn_data->msg, conn_data->request_len);
        resp_msg = std::string(conn_data->msg + conn_data->request_len, conn_data->response_len);
    }
};

/// record ///

class AbstractRecord {
public:
    virtual ~AbstractRecord() {}

    virtual AggregateType GetAggregateType() const = 0;
    virtual EventType GetEventType() const = 0;

    virtual std::string GetSpanName() = 0;

    uint64_t GetStartTimeStamp() { return start_ts_; }
    uint64_t GetEndTimeStamp() { return end_ts_; }
    double GetLatencyNs() const { return end_ts_ - start_ts_; }
    double GetLatencyMs() const { return (end_ts_ - start_ts_) / 1000; }
    void SetStartTs(uint64_t start_ts_ns) { start_ts_ = start_ts_ns; }
    void SetEndTs(uint64_t end_ts_ns) { end_ts_ = end_ts_ns; }
    int RollbackCount() const { return rollback_cnt_; }
    int Rollback() { return rollback_cnt_++; }

    virtual bool IsError() const = 0;
    virtual bool IsSlow() const = 0;
    virtual int GetStatusCode() const = 0;

    virtual DataTableSchema GetMetricsTableSchema() const = 0;

    virtual DataTableSchema GetTableSchema() const = 0;

    // virtual std::string GetMetricAttribute(size_t col) const =0;

protected:
    uint64_t start_ts_;
    uint64_t end_ts_;
    std::string span_name_;
    std::string span_kind_;
    int rollback_cnt_ = 0;
};


class AbstractNetRecord : public AbstractRecord {
public:
    ~AbstractNetRecord() override {}
    AggregateType GetAggregateType() const override { return AggregateType::NETWORK; }

    const ConnId GetConnId() const { return conn_id_; }

    std::string GetSpanName() override { return "xx"; }
    AbstractNetRecord(ConnId&& conn_id) : conn_id_(conn_id) {}

    AbstractNetRecord(const ConnId& conn_id) : conn_id_(conn_id) {}

protected:
    ConnId conn_id_;
};

class ConnStatsRecord : public AbstractNetRecord {
public:
    ~ConnStatsRecord() override {}
    ConnStatsRecord(ConnId&& conn_id) : AbstractNetRecord(std::move(conn_id)) {}
    ConnStatsRecord(const ConnId& conn_id) : AbstractNetRecord(conn_id) {}
    EventType GetEventType() const override { return EventType::CONN_STATS_EVENT; }

    bool IsError() const override { return false; }
    bool IsSlow() const override { return false; }
    int GetStatusCode() const override { return 0; }

    std::string GetSpanName() override { return "CONN_STATS"; }

    DataTableSchema GetMetricsTableSchema() const override { return kNetMetricsTable; }

    DataTableSchema GetTableSchema() const override { return kNetTable; }

    uint64_t drop_count_;
    uint64_t connect_sum_;
    uint64_t rtt_var_;
    uint64_t rtt_;
    uint64_t retrans_count_;
    uint64_t recv_packets_;
    uint64_t send_packets_;
    uint64_t recv_bytes_;
    uint64_t send_bytes_;
};

// AbstractAppRecord is intentionally designed to distinguish L5 and L7 Record of AbstractNetRecord. AbstractAppRecord
// is L7, while ConnStatsRecord is L5.
class AbstractAppRecord : public AbstractNetRecord {
public:
    AbstractAppRecord(ConnId&& conn_id) : AbstractNetRecord(std::move(conn_id)) {};

    void SetTraceId(const std::string& traceId) { mTraceId = traceId; }
    void SetSpanId(const std::string& spanId) { mSpanId = spanId; }

    virtual std::string GetReqBody() = 0;
    virtual std::string GetRespBody() = 0;
    virtual std::string GetMethod() = 0;
    virtual HeadersMap GetReqHeaderMap() = 0;
    virtual HeadersMap GetRespHeaderMap() = 0;
    virtual std::string GetProtocolVersion() = 0;
    virtual std::string GetPath() = 0;

    DataTableSchema GetMetricsTableSchema() const override { return kAppMetricsTable; }

    mutable std::string mTraceId;
    mutable std::string mSpanId;
};


class HttpRecord : public AbstractAppRecord {
public:
    ~HttpRecord() override {}
    HttpRecord(ConnId&& conn_id) : AbstractAppRecord(std::move(conn_id)) {}

    EventType GetEventType() const override { return EventType::HTTP_EVENT; }

    DataTableSchema GetTableSchema() const override { return kHTTPTable; }

    void SetPath(const std::string& path) { path_ = path; }

    void SetRealPath(const std::string& path) { real_path_ = path; }

    void SetReqBody(const std::string& body) { req_body = body; }

    void SetRespBody(const std::string& body) { resp_body = body; }

    void SetMethod(const std::string& method) { http_method = method; }

    void SetProtocolVersion(const std::string& version) { protocol_version = version; }

    void SetStatusCode(const std::string& code) {
        code_ = std::stoi(code);
        status_code = code;
    }

    void SetReqHeaderMap(HeadersMap& headerMap) { req_header_map = headerMap; }

    void SetRespHeaderMap(HeadersMap& headerMap) { resp_header_map = headerMap; }

    bool IsError() const override { return code_ >= 400; }

    // TODO @qianlu.kk
    bool IsSlow() const override { return GetLatencyMs() > 500; }
    int GetStatusCode() const override { return code_; }
    std::string GetReqBody() { return req_body; }
    std::string GetRespBody() { return resp_body; }
    std::string GetMethod() { return http_method; }
    HeadersMap GetReqHeaderMap() { return req_header_map; }
    HeadersMap GetRespHeaderMap() { return resp_header_map; }
    std::string GetProtocolVersion() { return protocol_version; }
    std::string GetPath() { return path_; }
    std::string GetRealPaht() { return real_path_; }
    std::string GetSpanName() override { return path_; }

private:
    std::string status_code;
    int code_;
    std::string path_;
    std::string real_path_;
    std::string conv_path_;
    std::string req_body;
    std::string resp_body;
    std::string http_method;
    std::string protocol_version;
    HeadersMap req_header_map;
    HeadersMap resp_header_map;
};

class AppRecord : public AbstractRecord {};

class MetricData {
public:
    virtual ~MetricData() {}
    MetricData(const ConnId& connId) : mConnId(connId) {}
    ConnId mConnId;
};

class AppMetricData : public MetricData {
public:
    AppMetricData(const ConnId& connId, const std::string& spanName) : MetricData(connId), mSpanName(spanName) {}
    ~AppMetricData() {}

    uint64_t mCount = 0;
    double mSum = 0;
    uint64_t mSlowCount = 0;
    uint64_t mErrCount = 0;
    uint64_t m2xxCount = 0;
    uint64_t m3xxCount = 0;
    uint64_t m4xxCount = 0;
    uint64_t m5xxCount = 0;

    std::string mAppId;
    std::string mAppName;
    std::string mHost;
    std::string mIp;
    std::string mNamespace;
    std::string mWorkloadName;
    std::string mWorkloadKind;
    std::string mDestId;
    std::string mEndpoint;
    std::string mRpcType;
    std::string mCallType;
    std::string mCallKind;
    std::string mSpanName;
};

class NetMetricData : public MetricData {
public:
    NetMetricData(const ConnId& connId) : MetricData(connId) {}
    ~NetMetricData() {}
    uint64_t mDropCount;
    uint64_t mRetransCount;
    uint64_t mRtt;
    uint64_t mRecvBytes;
    uint64_t mSendBytes;
    uint64_t mRecvPkts;
    uint64_t mSendPkts;
};

class AppSpanGroup {
public:
    AppSpanGroup() {}
    ~AppSpanGroup() {}

    std::vector<std::shared_ptr<AbstractAppRecord>> mRecords;
};

class AppLogGroup {
public:
    AppLogGroup() {}
    ~AppLogGroup() {}

    std::vector<std::shared_ptr<AbstractAppRecord>> mRecords;
};


} // namespace ebpf
} // namespace logtail


namespace std {
template <>
struct hash<logtail::ebpf::ProtocolType> {
    std::size_t operator()(const logtail::ebpf::ProtocolType& proto) const noexcept {
        return static_cast<std::size_t>(proto);
    }
};
} // namespace std


namespace std {
template <>
struct hash<logtail::ebpf::ConnId> {
    std::size_t operator()(const logtail::ebpf::ConnId& k) const {
        std::size_t h1 = std::hash<int32_t>{}(k.fd);
        std::size_t h2 = std::hash<uint32_t>{}(k.tgid);
        std::size_t h3 = std::hash<uint64_t>{}(k.start);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};
} // namespace std
