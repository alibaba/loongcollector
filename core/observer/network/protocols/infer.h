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

#include <iostream>
#include <tuple>
#include <cstring>
#include <arpa/inet.h>
#include "observer/interface/type.h"
#include "observer/interface/protocol.h"
#include "interface/network.h"
#include "interface/helper.h"


namespace logtail {
enum class PostgreSqlTag {
    // Frontend (F) & Backend (B).
    CopyData = 'd',
    CopyDone = 'c',

    // F.
    Query = 'Q',
    CopyFail = 'f',
    Close = 'C',
    Bind = 'B',
    Passwd = 'p',
    Parse = 'P',
    Desc = 'D',
    Sync = 'S',
    Execute = 'E',

    // B.
    DataRow = 'D',
    ReadyForQuery = 'Z',
    CopyOutResponse = 'H',
    CopyInResponse = 'G',
    ErrResp = 'E',
    CmdComplete = 'C',
    CloseComplete = '3',
    BindComplete = '2',
    EmptyQuery = 'I',
    Key = 'K',
    Auth = 'R',
    ParseComplete = '1',
    ParamDesc = 't',
    RowDesc = 'T',
    NoData = 'n',

    Unknown = '\0',
};

}


static __inline MessageType
infer_http_message(const char* buf, int32_t count, const uint16_t srcPort, const uint16_t dstPort) {
    // Smallest HTTP response is 17 characters:
    // HTTP/1.1 200 OK\r\n
    // Smallest HTTP response is 16 characters:
    // GET x HTTP/1.1\r\n
    if (count < 16) {
        return MessageType_None;
    }
    if (srcPort == 80) {
        return MessageType_Response;
    } else if (dstPort == 80) {
        return MessageType_Request;
    }

    if (buf[0] == 'H' && buf[1] == 'T' && buf[2] == 'T' && buf[3] == 'P') {
        return MessageType_Response;
    }
    if (buf[0] == 'G' && buf[1] == 'E' && buf[2] == 'T') {
        return MessageType_Request;
    }
    if (buf[0] == 'H' && buf[1] == 'E' && buf[2] == 'A' && buf[3] == 'D') {
        return MessageType_Request;
    }
    if (buf[0] == 'P' && buf[1] == 'O' && buf[2] == 'S' && buf[3] == 'T') {
        return MessageType_Request;
    }
    if (buf[0] == 'P' && buf[1] == 'U' && buf[2] == 'T') {
        return MessageType_Request;
    }
    if (buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'E' && buf[4] == 'T' && buf[5] == 'E') {
        return MessageType_Request;
    }
    return MessageType_None;
}

static __inline MessageType infer_mysql_message(
    const char* buf, int32_t count, int32_t pktRealSize, const uint16_t srcPort, const uint16_t dstPort) {
    // MySQL packets start with a 3-byte packet length and a 1-byte packet number.
    // The 5th byte on a request contains a command that tells the type.
    if (count < 5) {
        return MessageType_None;
    }
    if (srcPort == 3306) {
        return MessageType_Response;
    } else if (dstPort == 3306) {
        return MessageType_Request;
    }

    int32_t pktLen = (uint8_t)buf[0] + ((uint8_t)buf[1] << 8) + ((uint8_t)buf[2] << 16);
    uint8_t pktNum = buf[3];
    uint8_t com = buf[4];

    // MySQL报文头一共4个字节，pktLen+4个字节=Payload的长度
    if (pktLen != pktRealSize - 4) { // mysql报文一般pktLen和pktRealSize是相等的
        if (pktNum == 1 && pktLen == 1) {
            return MessageType_Response; // result set response
        }
    } else {
        if (pktNum == 0) {
            if (com == 0x0a) {
                // server greeting
                return MessageType_Response;
            } else {
                return MessageType_Request;
            }
        } else if (pktNum == 1) {
            if (pktLen > 36 && count > 40 && buf[18] == 0 && buf[25] == 0) {
                // login request
                return MessageType_Request;
            } else {
                return MessageType_Response;
            }
        }
    }
    return MessageType_None;
}

static __inline bool is_pgsql_message(const char* buf, int32_t count, const uint16_t srcPort, const uint16_t dstPort) {
    if (count < 5) {
        // Regular Message: Tag(char), Length(int32)
        // StartUp Message: Length(int32), Major(int16),Minor(int16)
        return false;
    }

    if (srcPort == 5432 || dstPort == 5432) {
        return true;
    }

    {
        // check startup message
        static char pgsql[] = "\x00\x03\x00\x00";
        auto tag = static_cast<logtail::PostgreSqlTag>(buf[0]);
        if (tag == logtail::PostgreSqlTag::Unknown) {
            uint32_t len = ntohl(*(uint32_t*)buf);
            if (len == (uint32_t)count && memcmp(buf + 4, pgsql, 4) == 0) {
                return true;
            }
            return false;
        }
    }

    // check other message
    uint32_t pos = 0;
    while (true) {
        auto tag = static_cast<logtail::PostgreSqlTag>(buf[pos++]);
        if (pos + 4 > (uint32_t)count) {
            return false;
        }
        if (!(tag == logtail::PostgreSqlTag::CopyData || tag == logtail::PostgreSqlTag::CopyDone
              || tag == logtail::PostgreSqlTag::Query || tag == logtail::PostgreSqlTag::CopyFail
              || tag == logtail::PostgreSqlTag::Close || tag == logtail::PostgreSqlTag::Bind
              || tag == logtail::PostgreSqlTag::Passwd || tag == logtail::PostgreSqlTag::Parse
              || tag == logtail::PostgreSqlTag::Desc || tag == logtail::PostgreSqlTag::Sync
              || tag == logtail::PostgreSqlTag::Execute || tag == logtail::PostgreSqlTag::DataRow
              || tag == logtail::PostgreSqlTag::ReadyForQuery || tag == logtail::PostgreSqlTag::CopyOutResponse
              || tag == logtail::PostgreSqlTag::CopyInResponse || tag == logtail::PostgreSqlTag::ErrResp
              || tag == logtail::PostgreSqlTag::CmdComplete || tag == logtail::PostgreSqlTag::CloseComplete
              || tag == logtail::PostgreSqlTag::BindComplete || tag == logtail::PostgreSqlTag::EmptyQuery
              || tag == logtail::PostgreSqlTag::Key || tag == logtail::PostgreSqlTag::Auth
              || tag == logtail::PostgreSqlTag::ParseComplete || tag == logtail::PostgreSqlTag::ParamDesc
              || tag == logtail::PostgreSqlTag::RowDesc || tag == logtail::PostgreSqlTag::NoData)) {
            return false;
        }
        uint32_t len = ntohl(*(uint32_t*)(buf + pos));
        pos += len;
        if (pos == (uint32_t)count) {
            return true;
        }
        if (pos > (uint32_t)count) {
            return false;
        }
    }
}


static __inline MessageType
infer_dns_message(const char* buf, size_t count, const uint16_t srcPort, const uint16_t dstPort) {
    if (srcPort == 53) {
        return MessageType_Response;
    } else if (dstPort == 53) {
        return MessageType_Request;
    }

    const int kDNSHeaderSize = 12;

    // Use the maximum *guaranteed* UDP packet size as the max DNS message size.
    // UDP packets can be larger, but this is the typical maximum size for DNS.
    const int kMaxDNSMessageSize = 512;

    // Maximum number of resource records.
    // https://stackoverflow.com/questions/6794926/how-many-a-records-can-fit-in-a-single-dns-response
    const int kMaxNumRR = 25;

    if (count < kDNSHeaderSize || count > kMaxDNSMessageSize) {
        return MessageType_None;
    }

    const uint8_t* ubuf = (const uint8_t*)buf;

    uint16_t flags = (ubuf[2] << 8) + ubuf[3];
    uint16_t num_questions = (ubuf[4] << 8) + ubuf[5];
    uint16_t num_answers = (ubuf[6] << 8) + ubuf[7];
    uint16_t num_auth = (ubuf[8] << 8) + ubuf[9];
    uint16_t num_addl = (ubuf[10] << 8) + ubuf[11];

    bool qr = (flags >> 15) & 0x1;
    uint8_t opcode = (flags >> 11) & 0xf;
    uint8_t zero = (flags >> 6) & 0x1;

    if (zero != 0) {
        return MessageType_None;
    }

    if (opcode != 0) {
        return MessageType_None;
    }

    if (num_questions == 0 || num_questions > 10) {
        return MessageType_None;
    }

    uint32_t num_rr = num_questions + num_answers + num_auth + num_addl;
    if (num_rr > kMaxNumRR) {
        return MessageType_None;
    }

    return (qr == 0) ? MessageType_Request : MessageType_Response;
}

static __inline bool is_redis_message(const char* buf, int32_t count, const uint16_t srcPort, const uint16_t dstPort) {
    // Redis messages start with an one-byte type marker, and end with \r\n terminal sequence.
    if (count < 3) {
        return false;
    }
    if (srcPort == 6379 || dstPort == 6379) {
        return true;
    }

    const char first_byte = buf[0];

    if ( // Simple strings start with +
        first_byte != '+' &&
        // Errors start with -
        first_byte != '-' &&
        // Integers start with :
        first_byte != ':' &&
        // Bulk strings start with $
        first_byte != '$' &&
        // Arrays start with *
        first_byte != '*') {
        return false;
    }

    // The last two chars are \r\n, the terminal sequence of all Redis messages.
    if (buf[count - 2] != '\r') {
        return false;
    }
    if (buf[count - 1] != '\n') {
        return false;
    }

    return true;
}

static __inline std::tuple<ProtocolType, MessageType>
infer_protocol(PacketEventHeader* header, PacketType pktType, const char* pkt, int32_t pktSize, int32_t pktRealSize) {
    std::tuple<ProtocolType, MessageType> ret(ProtocolType_None, MessageType_None);
    if ((std::get<1>(ret) = infer_http_message(pkt, pktSize, header->SrcPort, header->DstPort)) != MessageType_None) {
        std::get<0>(ret) = ProtocolType_HTTP;
    } else if ((std::get<1>(ret) = infer_dns_message(pkt, pktSize, header->SrcPort, header->DstPort))
               != MessageType_None) {
        std::get<0>(ret) = ProtocolType_DNS;
    } else if ((std::get<1>(ret) = infer_mysql_message(pkt, pktSize, pktRealSize, header->SrcPort, header->DstPort))
               != MessageType_None) {
        std::get<0>(ret) = ProtocolType_MySQL;
    } else if (is_redis_message(pkt, pktSize, header->SrcPort, header->DstPort)) {
        // Redis协议 从data中无法区分MessageType，依赖tcp协议层面的判断
        std::get<0>(ret) = ProtocolType_Redis;
        std::get<1>(ret) = InferRequestOrResponse(pktType, header);
    } else if (is_pgsql_message(pkt, pktSize, header->SrcPort, header->DstPort)) {
        std::get<0>(ret) = ProtocolType_PgSQL;
        std::get<1>(ret) = InferRequestOrResponse(pktType, header);
    }

    return ret;
}