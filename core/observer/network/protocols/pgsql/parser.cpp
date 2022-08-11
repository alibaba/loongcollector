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


#include "network/protocols/pgsql/parser.h"
#include "Logger.h"
#include "interface/helper.h"

namespace logtail {

ParseResult PgSQLProtocolParser::OnPacket(PacketType pktType,
                                          MessageType msgType,
                                          PacketEventHeader* header,
                                          const char* pkt,
                                          int32_t pktSize,
                                          int32_t pktRealSize) {
    PgSQLParser pgsql(pkt, pktSize);
    LOG_TRACE(sLogger,
              ("message_type", MessageTypeToString(msgType))("pgsql date", charToHexString(pkt, pktSize, pktSize)));
    try {
        pgsql.parse(msgType);
    } catch (const std::runtime_error& re) {
        LOG_DEBUG(sLogger,
                  ("pgsql_parse_fail", re.what())("data", charToHexString(pkt, pktSize, pktSize))(
                      "srcPort", header->SrcPort)("dstPort", header->DstPort));
        return ParseResult_Fail;
    } catch (const std::exception& ex) {
        LOG_DEBUG(sLogger,
                  ("pgsql_parse_fail", ex.what())("data", charToHexString(pkt, pktSize, pktSize))(
                      "srcPort", header->SrcPort)("dstPort", header->DstPort));
        return ParseResult_Fail;
    } catch (...) {
        LOG_DEBUG(
            sLogger,
            ("pgsql_parse_fail", "Unknown failure occurred when parse")("data", charToHexString(pkt, pktSize, pktSize))(
                "srcPort", header->SrcPort)("dstPort", header->DstPort));
        return ParseResult_Fail;
    }

    if (pgsql.OK()) {
        bool insertSuccess = true;
        switch (pgsql.type) {
            case PgSQLPacketType::Query: {
                insertSuccess = mCache.InsertReq([&](PgSQLRequestInfo* info) {
                    info->TimeNano = header->TimeNano;
                    info->ReqBytes = pktRealSize;
                    info->SQL = pgsql.query.sql.ToString();
                    LOG_TRACE(sLogger, ("pgsql insert req", info->ToString()));
                });
                break;
            }
            case PgSQLPacketType::IgnoreQuery: {
                insertSuccess = mCache.InsertReq([&](PgSQLRequestInfo* info) {
                    info->TimeNano = 0;
                    info->ReqBytes = PGSQL_REQUEST_INFO_IGNORE_FLAG;
                    info->SQL = "";
                    LOG_TRACE(sLogger, ("pgsql insert req", info->ToString()));
                });
                break;
            }
            case PgSQLPacketType::Response: {
                insertSuccess = mCache.InsertResp([&](PgSQLResponseInfo* info) {
                    info->TimeNano = header->TimeNano;
                    info->RespBytes = pktRealSize;
                    info->OK = pgsql.resp.ok;
                    LOG_TRACE(sLogger, ("pgsql insert resp", info->ToString()));
                });
                break;
            }
            default: {
                // do noting
            }
        }
        return insertSuccess ? ParseResult_OK : ParseResult_Drop;
    }
    return ParseResult_Fail;
}

bool PgSQLProtocolParser::GarbageCollection(size_t size_limit_bytes, uint64_t expireTimeNs) {
    return mCache.GarbageCollection(expireTimeNs);
}


int32_t PgSQLProtocolParser::GetCacheSize() {
    return this->mCache.GetRequestsSize() + this->mCache.GetResponsesSize();
}


std::ostream& operator<<(std::ostream& os, const PgSQLRequestInfo& info) {
    os << "TimeNano: " << info.TimeNano << " SQL: " << info.SQL << " ReqBytes: " << info.ReqBytes;
    return os;
}

std::ostream& operator<<(std::ostream& os, const PgSQLResponseInfo& info) {
    os << "TimeNano: " << info.TimeNano << " OK: " << info.OK << " RespBytes: " << info.RespBytes;
    return os;
}
} // namespace logtail