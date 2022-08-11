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
#include <stdint.h>
#include <vector>
#include <iostream>

#include "network/protocols/utils.h"

#define DNS_FLAG_RESPONSE 0x8000
#define DNS_FLAG_OPCODE(FLGS) ((FLGS >> 11) & 0x0F)

namespace logtail {

struct DNSHeader {
    uint16_t txid;
    uint16_t flags;
    uint16_t numQueries;
    uint16_t numAnswers;
    uint16_t numAuth{0};
    uint16_t numAddl{0};
};

enum DNSPacketType {
    DNSPacketRequest,
    DNSPacketResponse,
};

enum DNSQueryType {
    DNSQueryTypeA = 1, // ipv4 address query
    DNSQueryTypeCNAME = 5, // query cname
    DNSQueryTypePTR = 12, // reverse dns parse
    DNSQueryTypeHTTPS = 65, // query https address
    DNSQueryTypeMX = 15, // query mx
    DNSQueryTypeTXT = 16, // query txt
    DNSQueryTypeAAAA = 28, // query ipv6
};

inline const char* ToString(DNSQueryType v) {
    switch (v) {
        case DNSQueryTypeA:
            return "A";
        case DNSQueryTypeCNAME:
            return "CNAME";
        case DNSQueryTypeHTTPS:
            return "HTTPS";
        case DNSQueryTypeMX:
            return "MX";
        case DNSQueryTypeTXT:
            return "TXT";
        case DNSQueryTypeAAAA:
            return "AAAA";
        default:
            return "UnKnown";
    }
}

struct DNSRequest {
    std::string queryHost;
    DNSQueryType queryType;

public:
    void print() {
        std::cout << "queryHost: " << queryHost;
        std::cout << "queryType: " << queryType << std::endl;
    }
};

struct DNSRequestPacket {
    std::vector<DNSRequest> requests;
};

struct DNSResponse {
    std::string queryHost;
    DNSQueryType queryType;
    uint32_t ttlSeconds;
    std::string value;

public:
    void print() {
        std::cout << "-------------" << std::endl;
        std::cout << "queryHost: " << queryHost;
        std::cout << "queryType: " << queryType << std::endl;
        std::cout << "ttlSeconds: " << ttlSeconds << std::endl;
        std::cout << "value: " << value << std::endl;
    }
};

struct DNSResponsePacket {
    std::vector<DNSResponse> responses;
    uint32_t answerCount;
};

class DNSParser : public ProtoParser {
public:
    DNSParser(const char* payload, const size_t pktSize) : ProtoParser(payload, pktSize, true) {}

    ~DNSParser() {}

    void parse();

    void print();

private:
    void parseName(std::string& name, const bool advancePos);

    void parseHeader();

    void parseQuery();

    void parseAnswer();

public:
    // dns spec
    DNSHeader dnsHeader;

    DNSPacketType packetType;

    DNSRequestPacket dnsRequest;

    DNSResponsePacket dnsResponse;
};

} // namespace logtail