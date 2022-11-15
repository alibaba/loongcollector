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
#include <string.h>
#include "parser.h"
#include "inner_parser.h"
#include "observer/interface/helper.h"
#include "logger/Logger.h"

namespace logtail {

ParseResult HTTPProtocolParser::OnPacket(PacketType pktType,
                                         MessageType msgType,
                                         PacketEventHeader* header,
                                         const char* pkt,
                                         int32_t pktSize,
                                         int32_t pktRealSize) {
    // Currently, only accept packets with header.
    //        if (msgType == MessageType_Request) {
    //            if (reqCategory == HTTPBodyPacketCategory::Chunked) {
    //                int res = HTTPParser::isChunkedMsg(pkt, pktSize);
    //                if (res == 0) {
    //                    reqCategory = HTTPBodyPacketCategory::Unkonwn;
    //                    return ParseResult_Partial;
    //                } else if (res > 0) {
    //                    return ParseResult_Partial;
    //                } else {
    //                    reqCategory = HTTPBodyPacketCategory::Unkonwn;
    //                }
    //            } else if (reqCategory == HTTPBodyPacketCategory::MoreContent) {
    //                if (pktRealSize == mReqMoreContentCapacity) {
    //                    reqCategory = HTTPBodyPacketCategory::Unkonwn;
    //                    mReqMoreContentCapacity=0;
    //                    return ParseResult_Partial;
    //                } else if (pktRealSize < mReqMoreContentCapacity) {
    //                    mReqMoreContentCapacity -= pktRealSize;
    //                    return ParseResult_Partial;
    //                } else {
    //                    reqCategory = HTTPBodyPacketCategory::Unkonwn;
    //                }
    //            }
    //        }else if(msgType == MessageType_Response){
    //            if (respCategory == HTTPBodyPacketCategory::Chunked) {
    //                int res = HTTPParser::isChunkedMsg(pkt, pktSize);
    //                if (res == 0) {
    //                    mRespMoreContentCapacity=0;
    //                    respCategory = HTTPBodyPacketCategory::Unkonwn;
    //                    return ParseResult_Partial;
    //                } else if (res > 0) {
    //                    return ParseResult_Partial;
    //                } else {
    //                    respCategory = HTTPBodyPacketCategory::Unkonwn;
    //                }
    //            } else if (respCategory == HTTPBodyPacketCategory::MoreContent) {
    //                if (pktRealSize == mRespMoreContentCapacity) {
    //                    respCategory = HTTPBodyPacketCategory::Unkonwn;
    //                    return ParseResult_Partial;
    //                } else if (pktRealSize < mRespMoreContentCapacity) {
    //                    mRespMoreContentCapacity -= pktRealSize;
    //                    return ParseResult_Partial;
    //                } else {
    //                    respCategory = HTTPBodyPacketCategory::Unkonwn;
    //                }
    //            }
    //        }

    HTTPParser parser;
    if (msgType == MessageType_Request) {
        parser.ParseRequest(pkt, pktSize);
        //            reqCategory = parser.bodyPacketCategory;
        //            mReqMoreContentCapacity = parser.bodySize;
    } else if (msgType == MessageType_Response) {
        parser.ParseResp(pkt, pktSize);
        //            this->respCategory = parser.bodyPacketCategory;
        //            this->mRespMoreContentCapacity = parser.bodySize;
    }
    LOG_TRACE(sLogger,
              ("http got data", std::string(pkt, pktSize))("message_type", MessageTypeToString(msgType))(
                  "raw_data", charToHexString(pkt, pktSize, pktSize))("connection_id", header->SockHash));
    if (parser.status == -1) {
        LOG_DEBUG(sLogger,
                  ("http_parse_fail", "")("data", charToHexString(pkt, pktSize, pktSize))("srcPort", header->SrcPort)(
                      "dstPort", header->DstPort)("message_type", MessageTypeToString(msgType)));
        return ParseResult_Fail;
    }


    bool insertSuccess = true;
    if (msgType == MessageType_Request) {
        std::string host = parser.ReadHeaderVal("Host").ToString();
        if (host.empty()) {
            if (pktType == PacketType_Out) {
                host = SockAddressToString(header->DstAddr);
            } else {
                host = SockAddressToString(header->SrcAddr);
            }
        }
        int pos = parser.packet.msg.req.url.Find('?');
        std::string url = std::string(parser.packet.msg.req.url.mPtr, pos == -1 ? parser.packet.msg.req.url.mLen : pos);
        insertSuccess = mCache.InsertReq([&](HTTPRequestInfo* req) {
            req->TimeNano = header->TimeNano;
            req->Method = parser.packet.msg.req.method.ToString();
            req->URL = std::move(url);
            req->Version = std::to_string(parser.packet.common.version);
            req->Host = std::move(host);
            req->ReqBytes = pktRealSize;
            LOG_TRACE(sLogger, ("http insert req hash", header->SockHash)("data", req->ToString()));
        });
    } else if (msgType == MessageType_Response) {
        insertSuccess = mCache.InsertResp([&](HTTPResponseInfo* info) {
            info->TimeNano = header->TimeNano;
            info->RespCode = parser.packet.msg.resp.code;
            info->RespBytes = pktRealSize;
            LOG_TRACE(sLogger, ("http insert resp hash", header->SockHash)("data", info->ToString()));
        });
    }
    return insertSuccess ? ParseResult_OK : ParseResult_Drop;
}

bool HTTPProtocolParser::GarbageCollection(size_t size_limit_bytes, uint64_t expireTimeNs) {
    return mCache.GarbageCollection(expireTimeNs);
}

int32_t HTTPProtocolParser::GetCacheSize() {
    return mCache.GetRequestsSize() + mCache.GetResponsesSize();
}

} // end of namespace logtail