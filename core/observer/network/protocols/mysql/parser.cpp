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
#include "parser.h"
#include "inner_parser.h"
#include "logger/Logger.h"
#include "interface/helper.h"

namespace logtail {

ParseResult MySQLProtocolParser::OnPacket(PacketType pktType,
                                          MessageType msgType,
                                          PacketEventHeader* header,
                                          const char* pkt,
                                          int32_t pktSize,
                                          int32_t pktRealSize) {
    if (pktRealSize == 4) {
        // skip 4 bytes packet because the packet would be appended to the next packet.
        return ParseResult_OK;
    }
    LOG_TRACE(sLogger, ("req size", mCache.GetRequestsSize())("resp size", mCache.GetResponsesSize()));
    MySQLParser mysql(pkt, pktSize);
    try {
        mysql.parse();
    } catch (const std::runtime_error& re) {
        LOG_DEBUG(sLogger,
                  ("mysql_parse_fail", re.what())("data", charToHexString(pkt, pktSize, pktSize))("pid", header->PID)(
                      "msgType", MessageTypeToString(msgType))("srcPort", header->SrcPort)("dstPort", header->DstPort));
        return ParseResult_Fail;
    } catch (const std::exception& ex) {
        LOG_DEBUG(sLogger,
                  ("mysql_parse_fail", ex.what())("data", charToHexString(pkt, pktSize, pktSize))("pid", header->PID)(
                      "msgType", MessageTypeToString(msgType))("srcPort", header->SrcPort)("dstPort", header->DstPort));
        return ParseResult_Fail;
    } catch (...) {
        LOG_DEBUG(sLogger,
                  ("mysql_parse_fail", "Unknown failure occurred when parse")(
                      "data", charToHexString(pkt, pktSize, pktSize))("pid", header->PID)(
                      "msgType", MessageTypeToString(msgType))("srcPort", header->SrcPort)("dstPort", header->DstPort));
        return ParseResult_Fail;
    }
    if (mysql.OK()) {
        bool insertSuccess = true;
        LOG_TRACE(
            sLogger,
            ("hashID", header->SockHash)("packet_type", MySQLPacketTypeToString(mysql.mysqlPacketType))(
                "pid", header->PID)("msgType", MessageTypeToString(msgType))(
                "mysql_query", mysql.mysqlPacketQuery.sql.ToString())("mysql_resp", mysql.mysqlPacketResponse.ok));
        switch (mysql.mysqlPacketType) {
            case MySQLPacketTypeServerGreeting:
            case MySQLPacketNoResponseStatement:
                // do nothing when receive single direction packet.
                break;
            case MySQLPacketTypeClientLogin:
            case MySQLPacketIgnoreStatement: {
                insertSuccess = mCache.InsertReq([&](MySQLRequestInfo* info) {
                    info->TimeNano = 0;
                    info->ReqBytes = MYSQL_REQUEST_INFO_IGNORE_FLAG;
                    info->SQL = "";
                });
                break;
            }
            case MySQLPacketTypeCollectStatement: {
                insertSuccess = mCache.InsertReq([&](MySQLRequestInfo* info) {
                    info->TimeNano = header->TimeNano;
                    info->ReqBytes = pktRealSize;
                    info->SQL = mysql.mysqlPacketQuery.sql.ToString();
                });
                break;
            }
            case MySQLPacketTypeResponse: {
                insertSuccess = mCache.InsertResp([&](MySQLResponseInfo* info) {
                    info->TimeNano = header->TimeNano;
                    info->RespBytes = pktRealSize;
                    info->OK = mysql.mysqlPacketResponse.ok;
                });
                break;
            }
        }
        return insertSuccess ? ParseResult_OK : ParseResult_Drop;
    }
    LOG_DEBUG(
        sLogger,
        ("mysql_parse_fail", "Unknown packets when parse")("pid", header->PID)("msgType", MessageTypeToString(msgType))(
            "data", charToHexString(pkt, pktSize, pktSize))("srcPort", header->SrcPort)("dstPort", header->DstPort));
    return ParseResult_Fail;
}

bool MySQLProtocolParser::GarbageCollection(size_t size_limit_bytes, uint64_t expireTimeNs) {
    return mCache.GarbageCollection(expireTimeNs);
}

int32_t MySQLProtocolParser::GetCacheSize() {
    return this->mCache.GetRequestsSize() + this->mCache.GetResponsesSize();
}

} // end of namespace logtail