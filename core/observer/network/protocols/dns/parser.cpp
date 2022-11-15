// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <map>
#include <utility>
#include "parser.h"
#include "logger/Logger.h"
#include "interface/helper.h"
#include "StringTools.h"
#include "metas/ServiceMetaCache.h"
#include <string>

namespace logtail {

ParseResult DNSProtocolParser::OnPacket(PacketType pktType,
                                        MessageType msgType,
                                        PacketEventHeader* header,
                                        const char* pkt,
                                        int32_t pktSize,
                                        int32_t pktRealSize) {
    DNSParser dns(pkt, pktSize);
    try {
        dns.parse();
    } catch (const std::runtime_error& re) {
        LOG_DEBUG(sLogger,
                  ("dns_parse_fail", re.what())("data", charToHexString(pkt, pktSize, pktSize))(
                      "srcPort", header->SrcPort)("dstPort", header->DstPort));
        return ParseResult_Fail;
    } catch (const std::exception& ex) {
        LOG_DEBUG(sLogger,
                  ("dns_parse_fail", ex.what())("data", charToHexString(pkt, pktSize, pktSize))(
                      "srcPort", header->SrcPort)("dstPort", header->DstPort));
        return ParseResult_Fail;
    } catch (...) {
        LOG_DEBUG(
            sLogger,
            ("dns_parse_fail", "Unknown failure occurred when parse")("data", charToHexString(pkt, pktSize, pktSize))(
                "srcPort", header->SrcPort)("dstPort", header->DstPort));
        return ParseResult_Fail;
    }
    if (dns.OK()) {
        bool insertSuccess = true;
        uint16_t txid = dns.dnsHeader.txid;
        if (dns.packetType == DNSPacketRequest && !dns.dnsRequest.requests.empty()) {
            auto resp = mRespCache.find(txid);
            std::string queryHosts;
            std::string queryTypes;

            if (dns.dnsRequest.requests.size() == 1) { // 大部分情况
                queryHosts = dns.dnsRequest.requests.at(0).queryHost;
                queryTypes = ToString(dns.dnsRequest.requests.at(0).queryType);
            } else {
                for (auto iter = dns.dnsRequest.requests.begin(); iter < dns.dnsRequest.requests.end(); iter++) {
                    queryHosts.append(std::string(iter->queryHost)).append(",");
                    queryTypes.append(std::string(ToString(iter->queryType))).append(",");
                }
                // remove last ,
                if (!queryHosts.empty()) {
                    queryHosts.pop_back();
                }
                if (!queryTypes.empty()) {
                    queryTypes.pop_back();
                }
            }

            auto req = new DNSRequestInfo(header->TimeNano, std::move(queryHosts), std::move(queryTypes), pktRealSize);

            if (resp == mRespCache.end()) {
                mReqCache.insert(std::make_pair(txid, req));
            } else {
                insertSuccess = stitcher(req, resp->second);
                delete req;
                delete resp->second;
                mRespCache.erase(resp);
            }
        } else if (dns.packetType == DNSPacketResponse) {
            // currently, only collect the dns host outside the kubernetes cluster
            const static std::string sKubernetesDnsFlag = "cluster.local";
            const static auto sHostManager = ServiceMetaManager::GetInstance();
            for (auto& response : dns.dnsResponse.responses) {
                if (!EndWith(response.queryHost, sKubernetesDnsFlag)) {
                    sHostManager->AddHostName(header->PID, response.queryHost, response.value);
                }
            }
            auto resp = new DNSResponseInfo(header->TimeNano, dns.dnsResponse.answerCount, pktRealSize);
            auto req = mReqCache.find(txid);
            if (req == mReqCache.end()) {
                mRespCache.insert(std::make_pair(txid, resp));
            } else {
                insertSuccess = stitcher(req->second, resp);
                delete resp;
                delete req->second;
                mReqCache.erase(req);
            }
        }
        return insertSuccess ? ParseResult_OK : ParseResult_Drop;
    }
    return ParseResult_Fail;
}


DNSResponseInfo::DNSResponseInfo(uint64_t timeNano, uint16_t answerCount, int32_t respBytes)
    : TimeNano(timeNano), RespBytes(respBytes), AnswerCount(answerCount) {
}

DNSRequestInfo::DNSRequestInfo(uint64_t timeNano, std::string&& queryRecord, std::string&& queryType, int32_t reqBytes)
    : TimeNano(timeNano), ReqBytes(reqBytes), QueryRecord(queryRecord), QueryType(queryType) {
}


bool DNSProtocolParser::stitcher(DNSRequestInfo* requestInfo, DNSResponseInfo* responseInfo) {
    DNSProtocolEvent dnsEvent;
    dnsEvent.Key.ReqResource = std::move(requestInfo->QueryRecord);
    dnsEvent.Key.ReqType = std::move(requestInfo->QueryType);
    dnsEvent.Key.RespStatus = responseInfo->AnswerCount > 0 ? 1 : 0;
    dnsEvent.Key.ConnKey = mKey;
    dnsEvent.Info.LatencyNs = responseInfo->TimeNano - requestInfo->TimeNano;
    if (dnsEvent.Info.LatencyNs < 0) {
        dnsEvent.Info.LatencyNs = 0;
    }
    dnsEvent.Info.ReqBytes = requestInfo->ReqBytes;
    dnsEvent.Info.RespBytes = responseInfo->RespBytes;
    return mAggregator->AddEvent(std::move(dnsEvent));
}

bool DNSProtocolParser::GarbageCollection(size_t size_limit_bytes, uint64_t expireTimeNs) {
    for (auto item = mReqCache.begin(); item != mReqCache.end();) {
        if (item->second->TimeNano < expireTimeNs) {
            delete item->second;
            item = mReqCache.erase(item);
            continue;
        }
        item++;
    }

    for (auto item = mRespCache.begin(); item != mRespCache.end();) {
        if (item->second->TimeNano < expireTimeNs) {
            delete item->second;
            item = mRespCache.erase(item);
            continue;
        }
        item++;
    }
    return mRespCache.empty() && mReqCache.empty();
}

int32_t DNSProtocolParser::GetCacheSize() {
    return this->mReqCache.size() + this->mRespCache.size();
}

} // end of namespace logtail