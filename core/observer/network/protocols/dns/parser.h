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
#include "network/protocols/dns/type.h"
#include "observer/interface/network.h"
#include "inner_parser.h"

namespace logtail {


struct DNSRequestInfo {
    uint64_t TimeNano;
    int32_t ReqBytes;
    std::string QueryRecord;
    std::string QueryType;

public:
    DNSRequestInfo(uint64_t timeNano, std::string&& queryRecord, std::string&& queryType, int32_t reqBytes);
};

struct DNSResponseInfo {
    uint64_t TimeNano;
    int32_t RespBytes;
    uint16_t AnswerCount;

    DNSResponseInfo(uint64_t timeNano, uint16_t answerCount, int32_t respBytes);
};


// 协议解析器，流式解析，解析到某个协议后，自动放到aggregator中聚合
class DNSProtocolParser {
public:
    DNSProtocolParser(DNSProtocolEventAggregator* aggregator, PacketEventHeader* header)
        : mAggregator(aggregator), mKey(header) {}

    ~DNSProtocolParser() {
        for (auto& p : mReqCache) {
            delete p.second;
        }
        for (auto& p : mRespCache) {
            delete p.second;
        }
    }

    static DNSProtocolParser* Create(DNSProtocolEventAggregator* aggregator, PacketEventHeader* header) {
        return new DNSProtocolParser(aggregator, header);
    }

    static void Delete(DNSProtocolParser* parser) { delete parser; }

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
    bool stitcher(DNSRequestInfo* requestInfo, DNSResponseInfo* responseInfo);

    DNSProtocolEventAggregator* mAggregator = NULL;
    std::unordered_map<uint16_t, DNSRequestInfo*> mReqCache;
    std::unordered_map<uint16_t, DNSResponseInfo*> mRespCache;
    CommonAggKey mKey;

    friend class ProtocolDnsUnittest;
};

} // namespace logtail
