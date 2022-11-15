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

#include <string>
#include <memory>
#include <deque>
#include <ostream>
#include "network/protocols/redis/type.h"
#include "observer/interface/network.h"
#include "inner_parser.h"

namespace logtail {

struct RedisRequestInfo {
    uint64_t TimeNano;
    std::string CMD;
    int32_t ReqBytes;

    std::string ToString() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }
    friend std::ostream& operator<<(std::ostream& os, const RedisRequestInfo& info);
};

struct RedisResponseInfo {
    uint64_t TimeNano;
    bool isOK;
    int32_t RespBytes;
    std::string ToString() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }
    friend std::ostream& operator<<(std::ostream& os, const RedisResponseInfo& info);
};

typedef CommonCache<RedisRequestInfo, RedisResponseInfo, RedisProtocolEventAggregator, RedisProtocolEvent, 4>
    RedisCache;

// 协议解析器，流式解析，解析到某个协议后，自动放到aggregator中聚合
class RedisProtocolParser {
public:
    RedisProtocolParser(RedisProtocolEventAggregator* aggregator, PacketEventHeader* header)
        : mCache(aggregator), mKey(header) {
        mCache.BindConvertFunc([&](RedisRequestInfo* req, RedisResponseInfo* resp, RedisProtocolEvent& event) {
            event.Info.LatencyNs = resp->TimeNano - req->TimeNano;
            if (event.Info.LatencyNs < 0) {
                event.Info.LatencyNs = 0;
            }
            event.Info.ReqBytes = req->ReqBytes;
            event.Info.RespBytes = resp->RespBytes;
            event.Key.ConnKey = mKey;
            event.Key.QueryCmd = ToLowerCaseString(req->CMD.substr(0, req->CMD.find_first_of(' ')));
            event.Key.Query = std::move(req->CMD);
            event.Key.Status = resp->isOK;
            return true;
        });
    }

    static RedisProtocolParser* Create(RedisProtocolEventAggregator* aggregator, PacketEventHeader* header) {
        return new RedisProtocolParser(aggregator, header);
    }

    static void Delete(RedisProtocolParser* parser) { delete parser; }

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
    RedisCache mCache;
    CommonAggKey mKey;

    friend class ProtocolRedisUnittest;
};


} // namespace logtail
