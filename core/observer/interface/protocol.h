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

#include "interface/type.h"
#include <unordered_map>
#include <vector>

// 解析结果
enum ParseResult {
    ParseResult_OK,
    ParseResult_Fail,
    ParseResult_Drop,
    ParseResult_Partial,
};

// ProtocolEventKey 需要提供一个 uint64_t Hash() 函数

// 协议的事件定义，例如一次完整的HTTP请求和返回，一次DNS请求和返回
// 例如HTTP请求，记录：Host、URL、Method、Status、Latency、RequestSize、ResponseSize
template <typename ProtocolEventKey>
struct ProtocolEvent {
    // GetKey 获取聚合的Key（不需要做URL、Query的聚类）
    void GetKey(ProtocolEventKey& key) {}
};

// 数据包的聚合项，例如请求某一个HTTP协议，Key是Host、URL（聚类后）、Method、Status；ProtocolEventAggItem就记录
// 请求次数、总延迟、RequestSizeTotal、ResponseSizeTotal、PXX延迟的中间结果（待定）
// @TODO(yuanyi) : 这个可以设计成通用的
template <typename ProtocolEventKey, typename ProtocolEvent>
struct ProtocolEventAggItem {
    ProtocolEventKey& GetKey() { return ProtocolEventKey(); }
    // AddEvent 更新聚合结果
    void AddEvent(ProtocolEvent* event) {}
    // Clear 清理聚合结果
    void Clear() {}
};

// ProtocolPatternGenerator 协议分类器，只提供一个接口 ProtocolEventKey &
// GetPattern(ProtocolEventKey & key)

// 协议的聚类器
template <typename ProtocolEventKey,
          typename ProtocolEvent,
          typename ProtocolEventAggItem,
          typename ProtocolPatternGenerator>
class ProtocolEventAggregator {
public:
    ProtocolEventAggregator(ProtocolPatternGenerator& patternGenerator) : mPatternGenerator(patternGenerator) {}

    // AddEvent 增加一个事件
    void AddEvent(ProtocolEvent* event) {}

    // 获取AGG的结果，获取完毕后需要Clear ProtocolEventAggItem
    size_t GetAggResult(uint64_t timeNs, std::vector<ProtocolEventAggItem*>& aggList) { return size_t(0); }

protected:
    ProtocolPatternGenerator& mPatternGenerator;
    std::unordered_map<ProtocolEventKey*, ProtocolEventAggItem*> mProtocolEventAggMap;
};

// 协议解析器，流式解析，解析到某个协议后，自动放到aggregator中聚合
template <typename ProtocolEvent, typename ProtocolEventAggregator>
class ProtocolParser {
public:
    ProtocolParser(ProtocolEventAggregator* aggregator) {}

    // 创建一个实例
    static ProtocolParser* Create(ProtocolEventAggregator* aggregator) { return new ProtocolParser(aggregator); }

    // Packet到达的时候会Call，所有参数都会告诉类型（PacketType，MessageType）
    ParseResult OnPacket(PacketType pktType,
                         MessageType msgType,
                         uint64_t timeNs,
                         const char* pkt,
                         int32_t pktSize,
                         int32_t pktRealSize) {
        return ParseResult_OK;
    }

    // GC，把内部没有完成Event匹配的消息按照SizeLimit和TimeOut进行清理
    // 返回值，如果是true代表还有数据，false代表无数据
    bool GarbageCollection(size_t size_limit_bytes, uint64_t expireTimeNs) { return false; }
};
