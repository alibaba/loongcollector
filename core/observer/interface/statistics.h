/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "Monitor.h"
#include "iostream"
#include "monitor/LogtailAlarm.h"
#include <cstdint>
#include <sstream>

namespace logtail {

// Global statistics for container meta
struct ProcessMetaStatistic {
    uint32_t mCgroupPathTotalCount{0};
    uint32_t mCgroupPathParseFailCount{0};
    uint32_t mWatchProcessCount{0};
    uint32_t mFetchContainerMetaCount{0};
    uint32_t mFetchContainerMetaFailCount{0};

    static ProcessMetaStatistic* GetInstance() {
        static auto ptr = new ProcessMetaStatistic();
        return ptr;
    }

    static void Clear() {
        static auto sInstance = GetInstance();
        sInstance->doClear();
    }

    void FlushMetrics() {
        static auto sMonitor = LogtailMonitor::Instance();
        sMonitor->UpdateMetric("observer_processmeta_cgroup_count", mCgroupPathTotalCount);
        sMonitor->UpdateMetric("observer_processmeta_cgroup_invalid_count", mCgroupPathParseFailCount);
        sMonitor->UpdateMetric("observer_processmeta_watch_count", mWatchProcessCount);
        sMonitor->UpdateMetric("observer_processmeta_fetch_container_count", mFetchContainerMetaCount);
        sMonitor->UpdateMetric("observer_processmeta_fetch_container_fail_count", mFetchContainerMetaFailCount);
        doClear();
    }

    friend std::ostream& operator<<(std::ostream& os, const ProcessMetaStatistic& statistic) {
        os << "mCgroupPathTotalCount: " << statistic.mCgroupPathTotalCount
           << " mCgroupPathParseFailCount: " << statistic.mCgroupPathParseFailCount
           << " mWatchProcessCount: " << statistic.mWatchProcessCount
           << " mFetchContainerMetaCount: " << statistic.mFetchContainerMetaCount
           << " mFetchContainerMetaFailCount: " << statistic.mFetchContainerMetaFailCount;
        return os;
    }

    std::string ToString() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

private:
    ProcessMetaStatistic() = default;

    void doClear() {
        mCgroupPathTotalCount = 0;
        mCgroupPathParseFailCount = 0;
        mWatchProcessCount = 0;
        mFetchContainerMetaCount = 0;
        mFetchContainerMetaFailCount = 0;
    }
};

// Global statistics for netlink connection meta.
struct ConnectionMetaStatistic {
    uint16_t mGetSocketInfoCount{0};
    uint16_t mGetSocketInfoFailCount{0};
    uint16_t mGetNetlinkProberCount{0};
    uint16_t mGetNetlinkProberFailCount{0};
    uint16_t mFetchNetlinkCount{0};

    static ConnectionMetaStatistic* GetInstance() {
        static auto ptr = new ConnectionMetaStatistic();
        return ptr;
    }

    static void Clear() {
        static auto sInstance = GetInstance();
        sInstance->doClear();
    }

    void FlushMetrics() {
        static auto sMonitor = LogtailMonitor::Instance();
        sMonitor->UpdateMetric("observer_connmeta_socket_get_count", mGetSocketInfoCount);
        sMonitor->UpdateMetric("observer_connmeta_socket_get_fail_count", mGetSocketInfoFailCount);
        sMonitor->UpdateMetric("observer_connmeta_socket_create_prober_count", mGetNetlinkProberCount);
        sMonitor->UpdateMetric("observer_connmeta_socket_create_prober_fail_count", mGetNetlinkProberFailCount);
        sMonitor->UpdateMetric("observer_connmeta_socket_fetch_netlink_count", mFetchNetlinkCount);
        doClear();
    }

    friend std::ostream& operator<<(std::ostream& os, const ConnectionMetaStatistic& statistic) {
        os << "mGetSocketInfoCount: " << statistic.mGetSocketInfoCount
           << " mGetSocketInfoFailCount: " << statistic.mGetSocketInfoFailCount
           << " mGetNetlinkProberCount: " << statistic.mGetNetlinkProberCount
           << " mGetNetlinkProberFailCount: " << statistic.mGetNetlinkProberFailCount
           << " mFetchNetlinkCount: " << statistic.mFetchNetlinkCount;
        return os;
    }

    std::string ToString() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

private:
    ConnectionMetaStatistic() = default;

    void doClear() {
        mGetSocketInfoCount = 0;
        mGetSocketInfoFailCount = 0;
        mGetNetlinkProberCount = 0;
        mGetNetlinkProberFailCount = 0;
        mFetchNetlinkCount = 0;
    }
};

// Global statistics for network stat
struct NetworkStatistic {
    uint32_t mOutputEvents{0};
    uint32_t mOutputBytes{0};
    uint32_t mInputEvents{0};
    uint32_t mInputBytes{0};
    uint32_t mProtocolMatched{0};
    uint32_t mProtocolUnMatched{0};
    uint32_t mGCCount{0};
    uint32_t mGCReleaseConnCount{0};
    uint32_t mGCReleaseProcessCount{0};
    uint32_t mEbpfLostCount{0};
    uint32_t mEbpfGCCount{0};
    uint32_t mEbpfGCReleaseFDCount{0};
    uint32_t mEbpfDisableProcesses{0};
    uint32_t mEbpfUsingConnections{0};

    void FlushMetrics() {
        static auto sMonitor = LogtailMonitor::Instance();
        LogtailAlarm::GetInstance()->SendAlarm(OBSERVER_RUNTIME_ALARM,
                                               "ebpf lost event count: " + std::to_string(mEbpfLostCount));
        sMonitor->UpdateMetric("observer_input_events", mInputEvents);
        sMonitor->UpdateMetric("observer_input_bytes", mInputBytes);
        sMonitor->UpdateMetric("observer_output_events", mOutputEvents);
        sMonitor->UpdateMetric("observer_output_raw_bytes", mOutputBytes);
        sMonitor->UpdateMetric("observer_unmatched_protocol_event", mProtocolUnMatched);
        sMonitor->UpdateMetric("observer_matched_protocol_event", mProtocolMatched);
        sMonitor->UpdateMetric("observer_gc_count", mGCCount);
        sMonitor->UpdateMetric("observer_gc_release_conn_count", mGCReleaseConnCount);
        sMonitor->UpdateMetric("observer_gc_release_process_count", mGCReleaseProcessCount);
        sMonitor->UpdateMetric("observer_gc_ebpf_count", mEbpfGCCount);
        sMonitor->UpdateMetric("observer_gc_ebpf_release_fd_count", mEbpfGCReleaseFDCount);
        sMonitor->UpdateMetric("observer_ebpf_disable_processes", mEbpfDisableProcesses);
        sMonitor->UpdateMetric("observer_ebpf_holding_connections", mEbpfUsingConnections);
        sMonitor->UpdateMetric("observer_ebpf_lost_count", mEbpfLostCount);
        doClear();
    }

    std::string ToString() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const NetworkStatistic& statistic) {
        os << "mOutputEvents: " << statistic.mOutputEvents << " mOutputBytes: " << statistic.mOutputBytes
           << " mInputEvents: " << statistic.mInputEvents << " mInputBytes: " << statistic.mInputBytes
           << " mProtocolMatched: " << statistic.mProtocolMatched
           << " mProtocolUnMatched: " << statistic.mProtocolUnMatched << " mGCCount: " << statistic.mGCCount
           << " mGCReleaseConnCount: " << statistic.mGCReleaseConnCount
           << " mGCReleaseProcessCount: " << statistic.mGCReleaseProcessCount
           << " mEbpfLostCount: " << statistic.mEbpfLostCount << " mEbpfGCCount: " << statistic.mEbpfGCCount
           << " mEbpfGCReleaseFDCount: " << statistic.mEbpfGCReleaseFDCount
           << " mEbpfDisableProcesses: " << statistic.mEbpfDisableProcesses
           << " mEbpfUsingConnections: " << statistic.mEbpfUsingConnections;
        return os;
    }

    static NetworkStatistic* GetInstance() {
        static auto ptr = new NetworkStatistic();
        return ptr;
    }

    static void Clear() {
        static auto sInstance = GetInstance();
        sInstance->doClear();
    }

private:
    NetworkStatistic() = default;

    void doClear() {
        mOutputEvents = 0;
        mOutputBytes = 0;
        mInputEvents = 0;
        mInputBytes = 0;
        mProtocolMatched = 0;
        mProtocolUnMatched = 0;
        mGCCount = 0;
        mGCReleaseConnCount = 0;
        mGCReleaseProcessCount = 0;
        mEbpfGCCount = 0;
        mEbpfGCReleaseFDCount = 0;
        mEbpfDisableProcesses = 0;
        mEbpfUsingConnections = 0;
        mEbpfLostCount = 0;
    }
};

// Global statistics for protocol
struct ProtocolStatistic {
    uint32_t mHTTPParseFailCount{0};
    uint32_t mRedisParseFailCount{0};
    uint32_t mMySQLParseFailCount{0};
    uint32_t mPgSQLParseFailCount{0};
    uint32_t mDNSParseFailCount{0};
    uint32_t mHTTPDropCount{0};
    uint32_t mRedisDropCount{0};
    uint32_t mMySQLDropCount{0};
    uint32_t mPgSQLDropCount{0};
    uint32_t mDNSDropCount{0};
    uint32_t mHTTPCount{0};
    uint32_t mRedisCount{0};
    uint32_t mMySQLCount{0};
    uint32_t mPgSQLCount{0};
    uint32_t mDNSCount{0};

    static ProtocolStatistic* GetInstance() {
        static auto ptr = new ProtocolStatistic();
        return ptr;
    }

    static void Clear() {
        static auto sInstance = GetInstance();
        sInstance->doClear();
    }

    void FlushMetrics() {
        static auto sMonitor = LogtailMonitor::Instance();

        sMonitor->UpdateMetric("observer_protocol_http_drop_count", mHTTPDropCount);
        sMonitor->UpdateMetric("observer_protocol_dns_drop_count", mDNSDropCount);
        sMonitor->UpdateMetric("observer_protocol_mysql_drop_count", mMySQLDropCount);
        sMonitor->UpdateMetric("observer_protocol_pgsql_drop_count", mPgSQLDropCount);
        sMonitor->UpdateMetric("observer_protocol_redis_drop_count", mRedisDropCount);
        sMonitor->UpdateMetric("observer_protocol_http_count", mHTTPCount);
        sMonitor->UpdateMetric("observer_protocol_dns_count", mDNSCount);
        sMonitor->UpdateMetric("observer_protocol_mysql_count", mMySQLCount);
        sMonitor->UpdateMetric("observer_protocol_pgsql_count", mPgSQLCount);
        sMonitor->UpdateMetric("observer_protocol_redis_count", mRedisCount);
        sMonitor->UpdateMetric("observer_protocol_http_parse_fail_count", mHTTPParseFailCount);
        sMonitor->UpdateMetric("observer_protocol_dns_parse_fail_count", mDNSParseFailCount);
        sMonitor->UpdateMetric("observer_protocol_mysql_parse_fail_count", mMySQLParseFailCount);
        sMonitor->UpdateMetric("observer_protocol_pgsql_parse_fail_count", mPgSQLParseFailCount);
        sMonitor->UpdateMetric("observer_protocol_redis_parse_fail_count", mRedisParseFailCount);
        doClear();
    }

    std::string ToString() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const ProtocolStatistic& statistic) {
        os << "mHTTPParseFailCount: " << statistic.mHTTPParseFailCount
           << " mRedisParseFailCount: " << statistic.mRedisParseFailCount
           << " mMySQLParseFailCount: " << statistic.mMySQLParseFailCount
           << " mPgSQLParseFailCount: " << statistic.mPgSQLParseFailCount
           << " mDNSParseFailCount: " << statistic.mDNSParseFailCount << " mHTTPDropCount: " << statistic.mHTTPDropCount
           << " mRedisDropCount: " << statistic.mRedisDropCount << " mMySQLDropCount: " << statistic.mMySQLDropCount
           << " mPgSQLDropCount: " << statistic.mPgSQLDropCount << " mDNSDropCount: " << statistic.mDNSDropCount
           << " mHTTPCount: " << statistic.mHTTPCount << " mRedisCount: " << statistic.mRedisCount
           << " mMySQLCount: " << statistic.mMySQLCount << " mPgSQLCount: " << statistic.mPgSQLCount
           << " mDNSCount: " << statistic.mDNSCount;
        return os;
    }

private:
    ProtocolStatistic() = default;

    void doClear() {
        mHTTPParseFailCount = 0;
        mRedisParseFailCount = 0;
        mMySQLParseFailCount = 0;
        mPgSQLParseFailCount = 0;
        mDNSParseFailCount = 0;
        mHTTPDropCount = 0;
        mRedisDropCount = 0;
        mMySQLDropCount = 0;
        mPgSQLDropCount = 0;
        mDNSDropCount = 0;
        mHTTPCount = 0;
        mRedisCount = 0;
        mMySQLCount = 0;
        mPgSQLCount = 0;
        mDNSCount = 0;
    }
};

// Global statistics for analyse the memory usage of different protocol parsers.
struct ProtocolDebugStatistic {
    uint32_t mHTTPConnectionNum{0};
    uint32_t mHTTPConnectionCachedSize{0};
    uint32_t mDNSConnectionNum{0};
    uint32_t mDNSConnectionCachedSize{0};
    uint32_t mRedisConnectionNum{0};
    uint32_t mRedisConnectionCachedSize{0};
    uint32_t mMySQLConnectionNum{0};
    uint32_t mMySQLConnectionCachedSize{0};
    uint32_t mPgSQLConnectionNum{0};
    uint32_t mPgSQLConnectionCachedSize{0};

    static ProtocolDebugStatistic* GetInstance() {
        static auto ptr = new ProtocolDebugStatistic();
        return ptr;
    }

    static void Clear() {
        static auto sInstance = GetInstance();
        sInstance->doClear();
    }

    void FlushMetrics(bool flush) {
        static auto sMonitor = LogtailMonitor::Instance();

        if (flush) {
            sMonitor->UpdateMetric("observer_protocolstat_http_conn", mHTTPConnectionNum);
            sMonitor->UpdateMetric("observer_protocolstat_dns_conn", mDNSConnectionNum);
            sMonitor->UpdateMetric("observer_protocolstat_mysql_conn", mMySQLConnectionNum);
            sMonitor->UpdateMetric("observer_protocolstat_pgsql_conn", mPgSQLConnectionNum);
            sMonitor->UpdateMetric("observer_protocolstat_redis_conn", mRedisConnectionNum);
            sMonitor->UpdateMetric("observer_protocolstat_http_cached", mHTTPConnectionCachedSize);
            sMonitor->UpdateMetric("observer_protocolstat_dns_cached", mDNSConnectionCachedSize);
            sMonitor->UpdateMetric("observer_protocolstat_mysql_cached", mMySQLConnectionCachedSize);
            sMonitor->UpdateMetric("observer_protocolstat_pgsql_cached", mPgSQLConnectionCachedSize);
            sMonitor->UpdateMetric("observer_protocolstat_redis_cached", mRedisConnectionCachedSize);
        }
        doClear();
    }

    friend std::ostream& operator<<(std::ostream& os, const ProtocolDebugStatistic& statistic) {
        os << "mHTTPConnectionNum: " << statistic.mHTTPConnectionNum
           << " mHTTPConnectionCachedSize: " << statistic.mHTTPConnectionCachedSize
           << " mDNSConnectionNum: " << statistic.mDNSConnectionNum
           << " mDNSConnectionCachedSize: " << statistic.mDNSConnectionCachedSize
           << " mRedisConnectionNum: " << statistic.mRedisConnectionNum
           << " mRedisConnectionCachedSize: " << statistic.mRedisConnectionCachedSize
           << " mMySQLConnectionNum: " << statistic.mMySQLConnectionNum
           << " mMySQLConnectionCachedSize: " << statistic.mMySQLConnectionCachedSize
           << " mPgSQLConnectionNum: " << statistic.mPgSQLConnectionNum
           << " mPgSQLConnectionCachedSize: " << statistic.mPgSQLConnectionCachedSize;
        return os;
    }

    std::string ToString() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

private:
    ProtocolDebugStatistic() = default;

    void doClear() {
        mHTTPConnectionNum = 0;
        mHTTPConnectionCachedSize = 0;
        mDNSConnectionNum = 0;
        mDNSConnectionCachedSize = 0;
        mRedisConnectionNum = 0;
        mRedisConnectionCachedSize = 0;
        mMySQLConnectionNum = 0;
        mMySQLConnectionCachedSize = 0;
        mPgSQLConnectionNum = 0;
        mPgSQLConnectionCachedSize = 0;
    }
};

} // namespace logtail