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

#include <utility>
#include <ostream>

#include "network/protocols/pgsql/type.h"

#include "interface/network.h"
#include "inner_parser.h"

namespace logtail {

#define PGSQL_REQUEST_INFO_IGNORE_FLAG (-100)

struct PgSQLRequestInfo {
    uint64_t TimeNano;
    std::string SQL;
    int32_t ReqBytes;

    friend std::ostream& operator<<(std::ostream& os, const PgSQLRequestInfo& info);

    std::string ToString() {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }
};

struct PgSQLResponseInfo {
    uint64_t TimeNano;
    bool OK;
    int32_t RespBytes;

    friend std::ostream& operator<<(std::ostream& os, const PgSQLResponseInfo& info);

    std::string ToString() {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }
};

typedef CommonCache<PgSQLRequestInfo, PgSQLResponseInfo, PgSQLProtocolEventAggregator, PgSQLProtocolEvent, 4>
    PgsqlCache;


class PgSQLProtocolParser {
public:
    explicit PgSQLProtocolParser(PgSQLProtocolEventAggregator* aggregator, PacketEventHeader* header)
        : mCache(aggregator), mKey(header) {
        mCache.BindConvertFunc(
            [&](PgSQLRequestInfo* requestInfo, PgSQLResponseInfo* responseInfo, PgSQLProtocolEvent& event) -> bool {
                if (requestInfo->ReqBytes == PGSQL_REQUEST_INFO_IGNORE_FLAG) {
                    return false;
                }
                event.Info.LatencyNs = responseInfo->TimeNano - requestInfo->TimeNano;
                if (event.Info.LatencyNs < 0) {
                    event.Info.LatencyNs = 0;
                }
                event.Info.ReqBytes = requestInfo->ReqBytes;
                event.Info.RespBytes = responseInfo->RespBytes;
                event.Key.ConnKey = mKey;
                event.Key.QueryCmd = ToLowerCaseString(requestInfo->SQL.substr(0, requestInfo->SQL.find_first_of(' ')));
                event.Key.Query = std::move(requestInfo->SQL);
                event.Key.Status = responseInfo->OK;
                return true;
            });
    }

    static PgSQLProtocolParser* Create(PgSQLProtocolEventAggregator* aggregator, PacketEventHeader* header) {
        return new PgSQLProtocolParser(aggregator, header);
    }

    static void Delete(PgSQLProtocolParser* parser) { delete parser; }

    bool GarbageCollection(size_t size_limit_bytes, uint64_t expireTimeNs);

    int32_t GetCacheSize();

    ParseResult OnPacket(PacketType pktType,
                         MessageType msgType,
                         PacketEventHeader* header,
                         const char* pkt,
                         int32_t pktSize,
                         int32_t pktRealSize);


private:
    PgsqlCache mCache;
    CommonAggKey mKey;

    friend class ProtocolPgSqlUnittest;
};
} // namespace logtail
