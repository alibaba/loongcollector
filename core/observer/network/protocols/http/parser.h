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
#include <ostream>
#include "network/protocols/http/type.h"
#include "observer/interface/network.h"
#include "inner_parser.h"

namespace logtail {

struct HTTPRequestInfo {
    uint64_t TimeNano;
    std::string Method;
    std::string URL;
    std::string Version;
    std::string Host;
    int32_t ReqBytes;

    friend std::ostream& operator<<(std::ostream& os, const HTTPRequestInfo& info) {
        os << "TimeNano: " << info.TimeNano << " Method: " << info.Method << " URL: " << info.URL
           << " Version: " << info.Version << " Host: " << info.Host << " ReqBytes: " << info.ReqBytes;
        return os;
    }

    std::string ToString() {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }
};
struct HTTPResponseInfo {
    uint64_t TimeNano;
    int16_t RespCode;
    int32_t RespBytes;

    friend std::ostream& operator<<(std::ostream& os, const HTTPResponseInfo& info) {
        os << "TimeNano: " << info.TimeNano << " RespCode: " << info.RespCode << " RespBytes: " << info.RespBytes;
        return os;
    }
    std::string ToString() {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }
};

typedef CommonCache<HTTPRequestInfo, HTTPResponseInfo, HTTPProtocolEventAggregator, HTTPProtocolEvent, 4> HttpCache;


// 协议解析器，流式解析，解析到某个协议后，自动放到aggregator中聚合
class HTTPProtocolParser {
public:
    explicit HTTPProtocolParser(HTTPProtocolEventAggregator* aggregator, PacketEventHeader* header)
        : mCache(HttpCache(aggregator)), mKey(header) {
        mCache.BindConvertFunc(
            [&](HTTPRequestInfo* requestInfo, HTTPResponseInfo* responseInfo, HTTPProtocolEvent& event) -> bool {
                event.Info.LatencyNs = responseInfo->TimeNano - requestInfo->TimeNano;
                if (event.Info.LatencyNs < 0) {
                    event.Info.LatencyNs = 0;
                }
                event.Info.ReqBytes = requestInfo->ReqBytes;
                event.Info.RespBytes = responseInfo->RespBytes;
                event.Key.ReqType = std::move(requestInfo->Method);
                event.Key.ReqDomain = std::move(requestInfo->Host);
                event.Key.ReqResource = std::move(requestInfo->URL);
                event.Key.Version = std::move(requestInfo->Version);
                event.Key.RespCode = responseInfo->RespCode;
                event.Key.ConnKey = mKey;
                return true;
            });
    }

    static HTTPProtocolParser* Create(HTTPProtocolEventAggregator* aggregator, PacketEventHeader* header) {
        return new HTTPProtocolParser(aggregator, header);
    }

    static void Delete(HTTPProtocolParser* parser) { delete parser; }

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
    HttpCache mCache;
    CommonAggKey mKey;

    friend class ProtocolHttpUnittest;
};


} // end of namespace logtail
