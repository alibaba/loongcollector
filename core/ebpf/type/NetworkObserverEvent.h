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

#include <map>
#include <string>
#include <vector>

#include "ebpf/plugin/network_observer/Connection.h"
#include "ebpf/plugin/network_observer/Type.h"
#include "ebpf/type/table/AppTable.h"
#include "ebpf/type/table/HttpTable.h"
#include "ebpf/type/table/NetTable.h"
#include "logger/Logger.h"

namespace logtail {
namespace ebpf {

class Connection;

// class NetDataEvent {
// public:
//     std::shared_ptr<Connection> mConnection;
//     uint64_t mStartTs;
//     uint64_t mEndTs;
//     support_proto_e mProtocol;
//     enum support_role_e mRole;
//     std::string mReqMsg;
//     std::string mRespMsg;

//     NetDataEvent(std::shared_ptr<Connection> connection) : mConnection(connection) {}

//     explicit NetDataEvent(struct conn_data_event_t* data)
//         : mStartTs(data->start_ts),
//           mEndTs(data->end_ts),
//           mProtocol(data->protocol),
//           mRole(data->role) {
//         mReqMsg = std::string(data->msg, data->request_len);
//         mRespMsg = std::string(data->msg + data->request_len, data->response_len);
//     }
// };

enum class RecordType {
    APP_RECORD,
    CONN_STATS_RECORD,
};

/// record ///
class AbstractRecord {
public:
    virtual ~AbstractRecord() {}
    virtual RecordType GetRecordType() = 0;
    virtual std::string GetSpanName() = 0;

    uint64_t GetStartTimeStamp() { return mStartTs; }
    uint64_t GetEndTimeStamp() { return mEndTs; }
    double GetLatencyNs() const { return mEndTs - mStartTs; }
    double GetLatencyMs() const { return (mEndTs - mStartTs) / 1000; }
    void SetStartTs(uint64_t startTsNs) { mStartTs = startTsNs; }
    void SetEndTs(uint64_t mEndTsns) { mEndTs = mEndTsns; }
    int RollbackCount() const { return mRollbackCount; }
    int Rollback() { return mRollbackCount++; }
    bool ShouldSample() const { return mIsSample; }
    void MarkSample() { mIsSample = true; }

    virtual bool IsError() const = 0;
    virtual bool IsSlow() const = 0;
    virtual int GetStatusCode() const = 0;

    virtual DataTableSchema GetMetricsTableSchema() const = 0;

    virtual DataTableSchema GetTableSchema() const = 0;

protected:
    uint64_t mStartTs;
    uint64_t mEndTs;
    bool mIsSample;
    int mRollbackCount = 0;
};


class AbstractNetRecord : public AbstractRecord {
public:
    ~AbstractNetRecord() override {}
    std::string GetSpanName() override { return ""; }
    RecordType GetRecordType() override { return RecordType::CONN_STATS_RECORD; }
    std::shared_ptr<Connection> GetConnection() const { return mConnection; }
    explicit AbstractNetRecord(std::shared_ptr<Connection> connection) : mConnection(connection) {}

protected:
    std::shared_ptr<Connection> mConnection;
};

class ConnStatsRecord : public AbstractNetRecord {
public:
    ~ConnStatsRecord() override {}
    ConnStatsRecord(std::shared_ptr<Connection> connection) : AbstractNetRecord(connection) {}
    RecordType GetRecordType() override { return RecordType::CONN_STATS_RECORD; }
    bool IsError() const override { return false; }
    bool IsSlow() const override { return false; }
    int GetStatusCode() const override { return 0; }

    std::string GetSpanName() override { return "CONN_STATS"; }
    DataTableSchema GetMetricsTableSchema() const override { return kNetMetricsTable; }
    DataTableSchema GetTableSchema() const override { return kNetTable; }
    uint64_t mDropCount;
    uint64_t mConnSum;
    uint64_t mRttVar;
    uint64_t mRtt;
    uint64_t mRetransCount;
    uint64_t mRecvPackets;
    uint64_t mSendPackets;
    uint64_t mRecvBytes;
    uint64_t mSendBytes;
};

// AbstractAppRecord is intentionally designed to distinguish L5 and L7 Record of AbstractNetRecord. AbstractAppRecord
// is L7, while ConnStatsRecord is L5.
class AbstractAppRecord : public AbstractNetRecord {
public:
    explicit AbstractAppRecord(std::shared_ptr<Connection> connection) : AbstractNetRecord(connection) {}

    void SetTraceId(const std::string& traceId) { mTraceId = traceId; }
    void SetSpanId(const std::string& spanId) { mSpanId = spanId; }

    RecordType GetRecordType() override { return RecordType::APP_RECORD; }

    virtual std::string GetReqBody() const = 0;
    virtual std::string GetRespBody() const = 0;
    virtual std::string GetMethod() const = 0;
    virtual const HeadersMap& GetReqHeaderMap() const = 0;
    virtual const HeadersMap& GetRespHeaderMap() const = 0;
    virtual std::string GetProtocolVersion() const = 0;
    virtual std::string GetPath() const = 0;

    DataTableSchema GetMetricsTableSchema() const override { return kAppMetricsTable; }

    mutable std::string mTraceId;
    mutable std::string mSpanId;
};

const static std::string STATUS_CODE_1XX = "1xx";
const static std::string STATUS_CODE_2XX = "2xx";
const static std::string STATUS_CODE_3XX = "3xx";
const static std::string STATUS_CODE_4XX = "4xx";
const static std::string STATUS_CODE_5XX = "5xx";


class HttpRecord : public AbstractAppRecord {
public:
    ~HttpRecord() override {}
    HttpRecord(std::shared_ptr<Connection> connection) : AbstractAppRecord(connection) {}

    DataTableSchema GetTableSchema() const override { return kHTTPTable; }

    void SetPath(const std::string& path) { mPath = path; }

    void SetRealPath(const std::string& path) { mRealPath = path; }

    void SetReqBody(const std::string& body) { mReqBody = body; }

    void SetRespBody(const std::string& body) { mRespBody = body; }

    void SetMethod(const std::string& method) { mHttpMethod = method; }

    void SetProtocolVersion(const std::string& version) { mProtocolVersion = version; }

    void SetStatusCode(const std::string& code) {
        mCode = std::stoi(code);
        mStatusCode = code;
    }

    void SetStatusCode(int code) {
        mCode = code;
        if (code < 200) {
            mStatusCode = STATUS_CODE_1XX;
        } else if (code < 300) {
            mStatusCode = STATUS_CODE_2XX;
        } else if (code < 400) {
            mStatusCode = STATUS_CODE_3XX;
        } else if (code < 500) {
            mStatusCode = STATUS_CODE_4XX;
        } else {
            mStatusCode = STATUS_CODE_5XX;
        }
    }

    void SetReqHeaderMap(HeadersMap&& headerMap) { mReqHeaderMap = std::move(headerMap); }

    void SetRespHeaderMap(HeadersMap&& headerMap) { mRespHeaderMap = std::move(headerMap); }

    void SetRespMsg(std::string&& msg) { mRespMsg = std::move(msg); }

    bool IsError() const override { return mCode >= 400; }

    // TODO @qianlu.kk
    bool IsSlow() const override { return GetLatencyMs() > 500; }
    int GetStatusCode() const override { return mCode; }
    std::string GetReqBody() const { return mReqBody; }
    std::string GetRespBody() const { return mRespBody; }
    std::string GetRespMsg() const { return mRespMsg; }
    size_t GetReqBodySize() const { return mReqBodySize; }
    size_t GetRespBodySize() const { return mRespBodySize; }
    std::string GetMethod() const { return mHttpMethod; }
    const HeadersMap& GetReqHeaderMap() const { return mReqHeaderMap; }
    const HeadersMap& GetRespHeaderMap() const { return mRespHeaderMap; }
    std::string GetProtocolVersion() const { return mProtocolVersion; }
    std::string GetPath() const { return mPath; }
    std::string GetRealPath() const { return mRealPath; }
    std::string GetSpanName() override { return mPath; }

    std::string mStatusCode;
    int mCode = 0;
    std::string mPath;
    std::string mRealPath;
    std::string mConvPath;
    std::string mReqBody;
    size_t mReqBodySize = 0;
    std::string mRespBody;
    size_t mRespBodySize = 0;
    std::string mHttpMethod;
    std::string mProtocolVersion;
    std::string mRespMsg;
    HeadersMap mReqHeaderMap;
    HeadersMap mRespHeaderMap;
};

class AppRecord : public AbstractRecord {};

class MetricData {
public:
    virtual ~MetricData() {}
    MetricData(std::shared_ptr<Connection> conn) : mConnection(conn) {}
    // ConnId mConnId;
    std::shared_ptr<Connection> mConnection;
};

class AppMetricData : public MetricData {
public:
    AppMetricData(std::shared_ptr<Connection> conn, const std::string& spanName)
        : MetricData(conn), mSpanName(spanName) {}
    ~AppMetricData() {}

    uint64_t mCount = 0;
    double mSum = 0;
    uint64_t mSlowCount = 0;
    uint64_t mErrCount = 0;
    uint64_t m2xxCount = 0;
    uint64_t m3xxCount = 0;
    uint64_t m4xxCount = 0;
    uint64_t m5xxCount = 0;

    // std::array<> ??
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
    NetMetricData(std::shared_ptr<Connection> conn) : MetricData(conn) {}
    ~NetMetricData() {}
    uint64_t mDropCount;
    uint64_t mRetransCount;
    uint64_t mRtt;
    uint64_t mRecvBytes;
    uint64_t mSendBytes;
    uint64_t mRecvPkts;
    uint64_t mSendPkts;
    // std::array<> attrs ??
};

class AppSpanGroup {
public:
    AppSpanGroup() {}
    ~AppSpanGroup() {}

    std::vector<std::shared_ptr<AbstractRecord>> mRecords;
};

class AppLogGroup {
public:
    AppLogGroup() {}
    ~AppLogGroup() {}

    std::vector<std::shared_ptr<AbstractRecord>> mRecords;
};


} // namespace ebpf
} // namespace logtail
