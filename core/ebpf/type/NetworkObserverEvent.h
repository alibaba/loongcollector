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
#include "ebpf/type/table/DataTable.h"
#include "ebpf/type/table/HttpTable.h"
#include "ebpf/type/table/NetTable.h"
#include "ebpf/type/table/StaticDataRow.h"
#include "logger/Logger.h"

namespace logtail {
namespace ebpf {

class Connection;

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
    double GetLatencyMs() const { return (mEndTs - mStartTs) / 1e6; }
    double GetLatencySeconds() const { return (mEndTs - mStartTs) / 1e9; }
    void SetStartTsNs(uint64_t startTsNs) { mStartTs = startTsNs; }
    void SetEndTsNs(uint64_t mEndTsns) { mEndTs = mEndTsns; }
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
    bool mIsSample = false;
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
    int mState;
    uint64_t mDropCount = 0;
    uint64_t mRttVar = 0;
    uint64_t mRtt = 0;
    uint64_t mRetransCount = 0;
    uint64_t mRecvPackets = 0;
    uint64_t mSendPackets = 0;
    uint64_t mRecvBytes = 0;
    uint64_t mSendBytes = 0;
};

// AbstractAppRecord is intentionally designed to distinguish L5 and L7 Record of AbstractNetRecord. AbstractAppRecord
// is L7, while ConnStatsRecord is L5.
class AbstractAppRecord : public AbstractNetRecord {
public:
    explicit AbstractAppRecord(std::shared_ptr<Connection> connection) : AbstractNetRecord(connection) {}
    ~AbstractAppRecord() override {}

    void SetTraceId(const std::string& traceId) { mTraceId = traceId; }
    void SetSpanId(const std::string& spanId) { mSpanId = spanId; }

    RecordType GetRecordType() override { return RecordType::APP_RECORD; }

    virtual std::string GetReqBody() const = 0;
    virtual std::string GetRespBody() const = 0;
    virtual size_t GetReqBodySize() const = 0;
    virtual size_t GetRespBodySize() const = 0;
    virtual std::string GetMethod() const = 0;
    virtual const HeadersMap& GetReqHeaderMap() const = 0;
    virtual const HeadersMap& GetRespHeaderMap() const = 0;
    virtual std::string GetProtocolVersion() const = 0;
    virtual std::string GetPath() const = 0;

    DataTableSchema GetMetricsTableSchema() const override { return kAppMetricsTable; }

    mutable std::string mTraceId;
    mutable std::string mSpanId;
};

class HttpRecord : public AbstractAppRecord {
public:
    static std::atomic_int sConstructCount;
    static std::atomic_int sDestructCount;
    ~HttpRecord() override { sDestructCount++; }
    HttpRecord(std::shared_ptr<Connection> connection) : AbstractAppRecord(connection) { sConstructCount++; }

    DataTableSchema GetTableSchema() const override { return kHTTPTable; }

    void SetPath(const std::string& path) { mPath = path; }

    void SetRealPath(const std::string& path) { mRealPath = path; }

    void SetReqBody(const std::string& body) { mReqBody = body; }

    void SetRespBody(const std::string& body) { mRespBody = body; }

    void SetMethod(const std::string& method) { mHttpMethod = method; }

    void SetProtocolVersion(const std::string& version) { mProtocolVersion = version; }

    void SetStatusCode(int code) { mCode = code; }

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

    int mCode = 0;
    size_t mReqBodySize = 0;
    size_t mRespBodySize = 0;
    std::string mPath;
    std::string mRealPath;
    std::string mConvPath;
    std::string mReqBody;
    std::string mRespBody;
    std::string mHttpMethod;
    std::string mProtocolVersion;
    std::string mRespMsg;
    HeadersMap mReqHeaderMap;
    HeadersMap mRespHeaderMap;
};

class MetricData {
public:
    virtual ~MetricData() {}
    MetricData(std::shared_ptr<Connection> conn) : mConnection(conn) {}
    // ConnId mConnId;
    std::shared_ptr<Connection> mConnection;
};

class AppMetricData : public MetricData {
public:
    AppMetricData(const std::shared_ptr<Connection>& conn,
                  const std::shared_ptr<SourceBuffer>& sourceBuffer,
                  const StringView& spanName)
        : MetricData(conn), mTags(sourceBuffer) {
        mTags.SetNoCopy<kRpc>(spanName);
    }
    ~AppMetricData() {}

    std::string ToString() const {
        std::string res;
        for (size_t i = 0; i < kAppMetricsNum; i++) {
            res += std::string(mTags[i]);
            res += ",";
        }
        res += std::to_string(mCount);
        res += ",";
        res += std::to_string(mSum);
        return res;
    }

    uint64_t mCount = 0;
    double mSum = 0;
    uint64_t mSlowCount = 0;
    uint64_t mErrCount = 0;
    uint64_t m2xxCount = 0;
    uint64_t m3xxCount = 0;
    uint64_t m4xxCount = 0;
    uint64_t m5xxCount = 0;

    StaticDataRow<&kAppMetricsTable> mTags;
};

#define LC_TCP_MAX_STATES 13
class NetMetricData : public MetricData {
public:
    NetMetricData(const std::shared_ptr<Connection>& conn, const std::shared_ptr<SourceBuffer>& sourceBuffer)
        : MetricData(conn), mTags(sourceBuffer) {}
    ~NetMetricData() {}
    std::string ToString() const {
        std::string res;
        for (size_t i = 0; i < kNetMetricsNum; i++) {
            res += std::string(mTags[i]);
            res += ",";
        }
        return res;
    }

    uint64_t mDropCount = 0;
    uint64_t mRetransCount = 0;
    uint64_t mRtt = 0;
    uint64_t mRttCount = 0;
    uint64_t mRecvBytes = 0;
    uint64_t mSendBytes = 0;
    uint64_t mRecvPkts = 0;
    uint64_t mSendPkts = 0;
    std::array<int, LC_TCP_MAX_STATES> mStateCounts = {0};
    StaticDataRow<&kNetMetricsTable> mTags;
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
