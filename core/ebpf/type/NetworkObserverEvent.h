// Copyright 2025 iLogtail Authors
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
#include "ebpf/type/CommonDataEvent.h"
#include "ebpf/type/table/AppTable.h"
#include "ebpf/type/table/DataTable.h"
#include "ebpf/type/table/HttpTable.h"
#include "ebpf/type/table/NetTable.h"
#include "ebpf/type/table/StaticDataRow.h"
#include "logger/Logger.h"

namespace logtail::ebpf {

class Connection;

enum class RecordType {
    APP_RECORD,
    CONN_STATS_RECORD,
};

class L7Record : public CommonEvent {
public:
    virtual ~L7Record() {}
    explicit L7Record(const std::shared_ptr<Connection>& conn, const std::shared_ptr<AppDetail>& appDetail)
        : CommonEvent(KernelEventType::L7_RECORD), mConnection(conn), mAppDetail(appDetail) {}
    PluginType GetPluginType() const override { return PluginType::NETWORK_OBSERVE; }

    void MarkSample() { mSample = true; }
    bool ShouldSample() { return mSample; }
    void SetStartTsNs(uint64_t ts) { mStartTs = ts; }
    void SetEndTsNs(uint64_t ts) { mEndTs = ts; }
    void SetConnection(const std::shared_ptr<Connection>& conn) { mConnection = conn; }
    [[nodiscard]] std::shared_ptr<Connection> GetConnection() const { return mConnection; }
    [[nodiscard]] std::shared_ptr<AppDetail> GetAppDetail() const { return mAppDetail; }
    void SetAppDetail(const std::shared_ptr<AppDetail>& detail) { mAppDetail = detail; }
    uint64_t GetStartTimeStamp() { return mStartTs; }
    uint64_t GetEndTimeStamp() { return mEndTs; }
    [[nodiscard]] double GetLatencyNs() const { return mEndTs - mStartTs; }
    [[nodiscard]] double GetLatencyMs() const { return (mEndTs - mStartTs) / 1e6; }
    [[nodiscard]] double GetLatencySeconds() const { return (mEndTs - mStartTs) / 1e9; }

    [[nodiscard]] virtual const std::string& GetSpanName() = 0;
    [[nodiscard]] virtual bool IsError() const = 0;
    [[nodiscard]] virtual bool IsSlow() const = 0;
    [[nodiscard]] virtual int GetStatusCode() const = 0;

    const std::array<uint64_t, 4>& GetTraceId() { return mTraceId; }
    const std::array<uint64_t, 2>& GetSpanId() { return mSpanId; }
    void SetTraceId(std::array<uint64_t, 4>&& traceId) { mTraceId = traceId; }
    void SetSpanId(std::array<uint64_t, 2>&& spanId) { mSpanId = spanId; }

private:
    std::shared_ptr<Connection> mConnection;
    std::shared_ptr<AppDetail> mAppDetail;
    uint64_t mStartTs;
    uint64_t mEndTs;
    bool mSample = false;
    mutable std::array<uint64_t, 4> mTraceId{};
    mutable std::array<uint64_t, 2> mSpanId{};
};

class HttpRecord : public L7Record {
public:
    HttpRecord(const std::shared_ptr<Connection>& conn, const std::shared_ptr<AppDetail>& appDetail)
        : L7Record(conn, appDetail) {}
    [[nodiscard]] virtual bool IsError() const override { return mCode >= 400; }
    [[nodiscard]] virtual bool IsSlow() const override { return GetLatencyMs() >= 500; }
    void SetStatusCode(int code) { mCode = code; }
    [[nodiscard]] virtual int GetStatusCode() const override { return mCode; }
    [[nodiscard]] virtual const std::string& GetSpanName() { return mPath; }

    const std::string& GetReqBody() const { return mReqBody; }
    const std::string& GetRespBody() const { return mRespBody; }
    std::string GetRespMsg() const { return mRespMsg; }
    size_t GetReqBodySize() const { return mReqBodySize; }
    size_t GetRespBodySize() const { return mRespBodySize; }
    const std::string& GetMethod() const { return mHttpMethod; }

    const HeadersMap& GetReqHeaderMap() const { return mReqHeaderMap; }
    const HeadersMap& GetRespHeaderMap() const { return mRespHeaderMap; }
    void SetReqHeaderMap(HeadersMap&& headerMap) { mReqHeaderMap = std::move(headerMap); }
    void SetRespHeaderMap(HeadersMap&& headerMap) { mRespHeaderMap = std::move(headerMap); }

    void SetProtocolVersion(const std::string& version) { mProtocolVersion = version; }
    const std::string& GetProtocolVersion() const { return mProtocolVersion; }
    const std::string& GetPath() const { return mPath; }
    const std::string& GetRealPath() const { return mRealPath; }
    void SetPath(const std::string& path) { mPath = path; }
    void SetRealPath(const std::string& path) { mRealPath = path; }

    void SetReqBody(const std::string& body) { mReqBody = body; }
    void SetRespBody(const std::string& body) { mRespBody = body; }
    void SetRespMsg(std::string&& msg) { mRespMsg = std::move(msg); }
    void SetMethod(const std::string& method) { mHttpMethod = method; }

    // private:
    int mCode = 0;
    size_t mReqBodySize = 0;
    size_t mRespBodySize = 0;
    std::string mPath;
    std::string mRealPath;
    std::string mReqBody;
    std::string mRespBody;
    std::string mHttpMethod;
    std::string mProtocolVersion;
    std::string mRespMsg;
    HeadersMap mReqHeaderMap;
    HeadersMap mRespHeaderMap;
};

class ConnStatsRecord : public CommonEvent {
public:
    [[nodiscard]] std::shared_ptr<Connection> GetConnection() const { return mConnection; }
    std::shared_ptr<Connection> mConnection;
    uint64_t mTimestamp;
    int mState = 0;
    uint64_t mDropCount = 0;
    uint64_t mRttVar = 0;
    uint64_t mRtt = 0;
    uint64_t mRetransCount = 0;
    uint64_t mRecvPackets = 0;
    uint64_t mSendPackets = 0;
    uint64_t mRecvBytes = 0;
    uint64_t mSendBytes = 0;
};

class MetricData {
public:
    virtual ~MetricData() {}
    explicit MetricData(std::shared_ptr<Connection>& conn) : mConnection(conn) {}
    std::shared_ptr<Connection> mConnection;
};

class AppMetricData : public MetricData {
public:
    AppMetricData(std::shared_ptr<Connection>& conn,
                  const std::shared_ptr<SourceBuffer>& sourceBuffer,
                  const StringView& spanName)
        : MetricData(conn), mTags(sourceBuffer) {
        mTags.SetNoCopy<kRpc>(spanName);
    }
    ~AppMetricData() {}

    [[nodiscard]] std::string ToString() const {
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
    NetMetricData(std::shared_ptr<Connection>& conn, const std::shared_ptr<SourceBuffer>& sourceBuffer)
        : MetricData(conn), mTags(sourceBuffer) {}
    ~NetMetricData() {}
    [[nodiscard]] std::string ToString() const {
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
    AppSpanGroup() = default;
    ~AppSpanGroup() {}

    std::vector<std::shared_ptr<CommonEvent>> mRecords;
};

class AppLogGroup {
public:
    AppLogGroup() = default;
    ~AppLogGroup() {}

    std::vector<std::shared_ptr<CommonEvent>> mRecords;
};


} // namespace logtail::ebpf
