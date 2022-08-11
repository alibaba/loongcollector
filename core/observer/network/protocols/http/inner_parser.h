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

#include <vector>
#include <unordered_map>
#include <iostream>

#include "common/protocol/picohttpparser/picohttpparser.h"
#include "network/protocols/utils.h"

namespace logtail {

enum class HTTPBodyPacketCategory {
    Unkonwn,
    Chunked,
    MoreContent,
    SingleContent,
};

struct HTTPRequestPacket {
    SlsStringPiece method;
    SlsStringPiece url;
};

struct HTTPResponsePacket {
    SlsStringPiece msg;
    int code;
};

struct HTTPCommonPacket {
    int version;
    size_t headersNum{50};
    struct phr_header headers[50]{};
};

struct Packet {
    union Msg {
        struct HTTPRequestPacket req;
        struct HTTPResponsePacket resp;

        Msg() {}
    } msg;

    HTTPCommonPacket common;
};


struct HTTPParser {
    void ParseRequest(const char* buf, size_t size) {
        packetLen = size;
        status = phr_parse_request(buf,
                                   size,
                                   &packet.msg.req.method.mPtr,
                                   &packet.msg.req.method.mLen,
                                   &packet.msg.req.url.mPtr,
                                   &packet.msg.req.url.mLen,
                                   &packet.common.version,
                                   packet.common.headers,
                                   &packet.common.headersNum,
                                   /*last_len*/ 0);
    }

    void ParseResp(const char* buf, size_t size) {
        packetLen = size;
        status = phr_parse_response(buf,
                                    size,
                                    &packet.common.version,
                                    &packet.msg.resp.code,
                                    &packet.msg.resp.msg.mPtr,
                                    &packet.msg.resp.msg.mLen,
                                    packet.common.headers,
                                    &packet.common.headersNum,
                                    /*last_len*/ 0);
    }

    void ParseBodyType() {
        // Content-Length
        static const std::string sContentName("Content-Length");
        auto val = ReadHeaderVal(sContentName);
        if (val.Size() > 0) {
            auto len = std::strtol(val.mPtr, NULL, 10);
            if (len <= long(packetLen - status)) {
                bodyPacketCategory = HTTPBodyPacketCategory::SingleContent;
            } else {
                bodySize = len;
                bodyPacketCategory = HTTPBodyPacketCategory::MoreContent;
            }
            return;
        }

        static const std::string sChunkedName("Transfer-Encoding");
        static const std::string sChunkedVal = "chunked";
        val = ReadHeaderVal(sChunkedName);
        if (val.Size() > 0 && val == sChunkedVal) {
            bodyPacketCategory = HTTPBodyPacketCategory::Chunked;
            return;
        }
    }

    SlsStringPiece ReadHeaderVal(const std::string& name) {
        for (size_t i = 0; i < this->packet.common.headersNum; ++i) {
            if (std::strncmp(packet.common.headers[i].name, name.c_str(), packet.common.headers[i].name_len) == 0) {
                return {packet.common.headers[i].value, packet.common.headers[i].value_len};
            }
        }
        return {};
    }

    static int isChunkedMsg(const char* buf, size_t size) {
        if (size > 4) {
            bool commonChunked = *(buf + size - 1) == '\n' && *(buf + size - 2) == '\r';
            bool endChunked = *(buf + size - 3) == '\n' && *(buf + size - 4) == '\r';
            if (commonChunked && endChunked) {
                return 0;
            }
            if (commonChunked) {
                return 1;
            }
            return -1;
        }
        return -1;
    }

    Packet packet;
    int status = 0;
    int bodySize = 0;
    size_t packetLen = 0;
    HTTPBodyPacketCategory bodyPacketCategory;

private:
};

} // end of namespace logtail