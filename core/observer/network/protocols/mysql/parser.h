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

#include <map>
#include <utility>
#include "network/protocols/mysql/type.h"
#include "observer/interface/network.h"
#include "inner_parser.h"

namespace logtail {


#define MYSQL_REQUEST_INFO_IGNORE_FLAG (-100)

struct MySQLRequestInfo {
    uint64_t TimeNano{0};
    int32_t ReqBytes{0};
    std::string SQL;
};

struct MySQLResponseInfo {
    uint64_t TimeNano{0};
    int32_t RespBytes{0};
    bool OK{false};
};

typedef CommonCache<MySQLRequestInfo, MySQLResponseInfo, MySQLProtocolEventAggregator, MySQLProtocolEvent, 4>
    MysqlCache;

// 协议解析器，流式解析，解析到某个协议后，自动放到aggregator中聚合
class MySQLProtocolParser {
public:
    explicit MySQLProtocolParser(MySQLProtocolEventAggregator* aggregator, PacketEventHeader* header)
        : mCache(aggregator), mKey(header) {
        mCache.BindConvertFunc(
            [&](MySQLRequestInfo* requestInfo, MySQLResponseInfo* responseInfo, MySQLProtocolEvent& event) -> bool {
                if (requestInfo->ReqBytes == MYSQL_REQUEST_INFO_IGNORE_FLAG) {
                    return false;
                }
                event.Info.LatencyNs = int64_t(responseInfo->TimeNano - requestInfo->TimeNano);
                event.Info.ReqBytes = requestInfo->ReqBytes;
                event.Info.RespBytes = responseInfo->RespBytes;
                event.Key.QueryCmd = ToLowerCaseString(requestInfo->SQL.substr(0, requestInfo->SQL.find_first_of(' ')));
                event.Key.Query = std::move(requestInfo->SQL);
                event.Key.Status = responseInfo->OK;
                event.Key.ConnKey = mKey;
                return true;
            });
    }

    static MySQLProtocolParser* Create(MySQLProtocolEventAggregator* aggregator, PacketEventHeader* header) {
        return new MySQLProtocolParser(aggregator, header);
    }

    static void Delete(MySQLProtocolParser* parser) { delete parser; }

    // Packet到达的时候会Call，所有参数都会告诉类型（PacketType，MessageType）
    ParseResult OnPacket(PacketType pktType,
                         MessageType msgType,
                         PacketEventHeader* header,
                         const char* pkt,
                         int32_t pktSize,
                         int32_t pktRealSize);

    // GC，把内部没有完成Event匹配的消息按照SizeLimit和TimeOut进行清理
    // 返回值，如果是true代表还有数据，false代表无数据
    bool GarbageCollection(size_t size_limit_bytes, uint64_t expireTimeNs);

    int32_t GetCacheSize();

private:
    MysqlCache mCache;
    CommonAggKey mKey;
    friend class ProtocolMySqlUnittest;
};

} // namespace logtail
