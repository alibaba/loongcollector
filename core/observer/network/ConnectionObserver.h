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

#include "interface/network.h"
#include <network/protocols/ProtocolEventAggregators.h>
#include <network/protocols/dns/parser.h>
#include <network/protocols/http/parser.h>
#include <network/protocols/mysql/parser.h>
#include <network/protocols/redis/parser.h>
#include <ostream>
#include "NetworkConfig.h"
#include "network/protocols/pgsql/parser.h"
#include "interface/statistics.h"

#define OBSERVER_PROTOCOL_GARBAGE(protocolType) \
    { \
        protocolType##ProtocolParser* parser = (protocolType##ProtocolParser*)mProtocolParser; \
        auto success = parser->GarbageCollection(size_limit_bytes, nowTimeNs); \
        if (!success && BOOL_FLAG(sls_observer_network_protocol_stat)) { \
            ++sStatistic->m##protocolType##ConnectionNum; \
            sStatistic->m##protocolType##ConnectionCachedSize += parser->GetCacheSize(); \
        } \
        return success; \
    }
#define OBSERVER_PROTOCOL_ON_DATA(protocolType) \
    { \
        if (mProtocolParser == NULL) { \
            mProtocolParser \
                = protocolType##ProtocolParser::Create(mAllAggregators.Get##protocolType##Aggregator(), header); \
        } \
        auto parser = (protocolType##ProtocolParser*)mProtocolParser; \
        ParseResult rst \
            = parser->OnPacket(data->PktType, data->MsgType, header, data->Buffer, data->BufferLen, data->RealLen); \
        if (rst == ParseResult_Fail) { \
            ++sStatistic->m##protocolType##ParseFailCount; \
        } \
        ++sStatistic->m##protocolType##Count; \
        if (rst == ParseResult_Drop) { \
            ++sStatistic->m##protocolType##DropCount; \
        } \
    }
namespace logtail {

class ConnectionObserver {
public:
    ConnectionObserver(PacketEventHeader* header, ProtocolEventAggregators& allAggregators)
        : mCreateReason(*header), mAllAggregators(allAggregators) {
        mLastDataTimeNs = header->TimeNano;
    }

    ~ConnectionObserver() { ClearParser(); }


    void ClearParser() {
        if (mProtocolParser == NULL) {
            return;
        }
        switch (mLastProtocolType) {
            case ProtocolType_None:
                break;
            case ProtocolType_HTTP:
                HTTPProtocolParser::Delete((HTTPProtocolParser*)mProtocolParser);
                break;
            case ProtocolType_DNS:
                DNSProtocolParser::Delete((DNSProtocolParser*)mProtocolParser);
                break;
            case ProtocolType_MySQL:
                MySQLProtocolParser::Delete((MySQLProtocolParser*)mProtocolParser);
                break;
            case ProtocolType_Redis:
                RedisProtocolParser::Delete((RedisProtocolParser*)mProtocolParser);
                break;
            case ProtocolType_PgSQL:
                PgSQLProtocolParser::Delete((PgSQLProtocolParser*)mProtocolParser);
                break;
            default:
                break;
        }
        mProtocolParser = NULL;
    }

    void OnData(PacketEventHeader* header, PacketEventData* data) {
        static auto sStatistic = ProtocolStatistic::GetInstance();
        mLastDataTimeNs = header->TimeNano;
        if (mLastProtocolType != ProtocolType_None && mLastProtocolType != data->PtlType) {
            ClearParser();
            ++mProtocolSwitchCount;
            if (mProtocolSwitchCount % 10 == 0) {
                // log error
            }
        }
        mLastProtocolType = data->PtlType;

        switch (data->PtlType) {
            case ProtocolType_None:
                break;
            case ProtocolType_HTTP:
                OBSERVER_PROTOCOL_ON_DATA(HTTP);
                break;
            case ProtocolType_DNS:
                OBSERVER_PROTOCOL_ON_DATA(DNS);
                break;
            case ProtocolType_MySQL:
                OBSERVER_PROTOCOL_ON_DATA(MySQL);
                break;
            case ProtocolType_Redis:
                OBSERVER_PROTOCOL_ON_DATA(Redis);
                break;
            case ProtocolType_PgSQL:
                OBSERVER_PROTOCOL_ON_DATA(PgSQL);
                break;
            default:
                break;
        }
    }
    void MarkDeleted() { mMarkDeleted = true; }


    bool GarbageCollection(size_t size_limit_bytes, uint64_t nowTimeNs) {
        auto sStatistic = ProtocolDebugStatistic::GetInstance();
        if (mMarkDeleted
            && nowTimeNs - mLastDataTimeNs
                > (uint64_t)INT64_FLAG(sls_observer_network_connection_closed_timeout) * 1000LL * 1000LL * 1000LL) {
            return true;
        }
        if (nowTimeNs - mLastDataTimeNs
            > (uint64_t)INT64_FLAG(sls_observer_network_connection_timeout) * 1000LL * 1000LL * 1000LL) {
            return true;
        }
        if (mProtocolParser == NULL) {
            return false;
        }
        switch (mLastProtocolType) {
            case ProtocolType_None:
                break;
            case ProtocolType_HTTP:
                OBSERVER_PROTOCOL_GARBAGE(HTTP);
                break;
            case ProtocolType_DNS:
                OBSERVER_PROTOCOL_GARBAGE(DNS);
                break;
            case ProtocolType_MySQL:
                OBSERVER_PROTOCOL_GARBAGE(MySQL);
                break;
            case ProtocolType_Redis:
                OBSERVER_PROTOCOL_GARBAGE(Redis);
                break;
            case ProtocolType_PgSQL:
                OBSERVER_PROTOCOL_GARBAGE(PgSQL);
                break;
            default:
                break;
        }
        return false;
    }

protected:
    PacketEventHeader mCreateReason;
    ProtocolEventAggregators& mAllAggregators;
    bool mMarkDeleted = false;
    ProtocolType mLastProtocolType = ProtocolType_None;
    int32_t mProtocolSwitchCount = 0;
    void* mProtocolParser = NULL;
    uint64_t mLastDataTimeNs = 0;

    friend class ProtocolDnsUnittest;
    friend class ProtocolHttpUnittest;
    friend class ProtocolMySqlUnittest;
    friend class ProtocolRedisUnittest;
    friend class ProtocolPgSqlUnittest;
};

} // namespace logtail
