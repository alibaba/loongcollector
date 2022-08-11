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

#include <iostream>

#include "inner_parser.h"
#include "interface/network.h"
#include "interface/helper.h"

namespace logtail {

void DNSParser::parseName(std::string& name, const bool advancePos) {
    bool useReferPosition = false;

    size_t readPos = currPostion;
    while (readPos < pktSize) {
        int nameSize = (int)payload[readPos];

        if (((nameSize >> 6) & 0x03) == 0x03) {
            // dns name refer to anthoer position
            readPos = ((payload[readPos] & 0x3f) << 8) + payload[readPos + 1];
            useReferPosition = true;
            continue;
        }
        if (nameSize == 0) {
            if (!name.empty()) {
                name.pop_back();
            }
            readPos += 1;
            break;
        } else if (nameSize < 0) {
            setParseFail("unexcepted nameSize found");
            return;
        }
        name.append(payload + readPos + 1, nameSize).append(".");
        readPos += nameSize + 1;
    }

    if (advancePos) {
        if (useReferPosition) {
            currPostion += 2;
        } else {
            currPostion = readPos;
        }
    }
}

void DNSParser::parseQuery() {
    dnsRequest.requests.reserve(dnsHeader.numQueries);
    for (int i = 0; i < dnsHeader.numQueries; i++) {
        DNSRequest request;
        parseName(request.queryHost, true);

        request.queryType = DNSQueryType(readUint16());
        positionCommit(2); // query class
        dnsRequest.requests.push_back(request);
    }
}

void DNSParser::parseAnswer() {
    dnsResponse.responses.reserve(dnsHeader.numAnswers);
    dnsResponse.answerCount = dnsHeader.numAnswers;
    for (int i = 0; i < dnsHeader.numAnswers; i++) {
        DNSResponse response;

        // DNSRecord  queryHost;
        parseName(response.queryHost, true);

        // read dns answser type
        response.queryType = DNSQueryType(readUint16());

        // read dns answer class
        positionCommit(2); // answer class

        // read ttl
        response.ttlSeconds = readUint32();

        // read data length
        int answerDataLen = readUint16();

        if (response.queryType == DNSQueryTypeA) {
            uint8_t* ipaddr = (uint8_t*)(payload + currPostion);
            positionCommit(answerDataLen);
            if (!isParseFail) {
                SockAddress addr;
                addr.Type = SockAddressType_IPV4;
                addr.Addr.IPV4 = *(uint32_t*)ipaddr;
                response.value = SockAddressToString(addr);
            }
        } else if (response.queryType == DNSQueryTypeAAAA) {
            uint8_t* ipaddr = (uint8_t*)(payload + currPostion);
            positionCommit(answerDataLen);
            if (!isParseFail) {
                SockAddress addr;
                addr.Type = SockAddressType_IPV6;
                addr.Addr.IPV6[0] = *(uint64_t*)ipaddr;
                addr.Addr.IPV6[1] = *(uint64_t*)(ipaddr + 8);
                response.value = SockAddressToString(addr);
            }
        } else if (response.queryType == DNSQueryTypeCNAME) {
            std::string data;
            parseName(data, false);
            positionCommit(answerDataLen);
            response.value = std::move(data);
        } else {
            response.value = ToString(response.queryType);
        }
        dnsResponse.responses.push_back(response);
    }
}

void DNSParser::print() {
    // print request
    std::cout << "****** Query ******" << std::endl;
    for (auto r : dnsRequest.requests) {
        r.print();
    }

    // print response
    std::cout << "****** Response ******" << std::endl;
    for (auto r : dnsResponse.responses) {
        r.print();
    }

    std::cout << std::endl;
}

void DNSParser::parseHeader() {
    if (pktSize < sizeof(dnsHeader)) {
        isParseFail = true;
        return;
    }

    dnsHeader.txid = readUint16();
    dnsHeader.flags = readUint16();
    dnsHeader.numQueries = readUint16();
    dnsHeader.numAnswers = readUint16();

    // skip auth rr(+2) and add rr(+2)
    positionCommit(4);

    if (DNS_FLAG_OPCODE(dnsHeader.flags) != 0) {
        setParseFail("not a stand query");
        return;
    }
    if (dnsHeader.numQueries == 0) {
        setParseFail("dns numQueries is zero");
        return;
    }

    if ((dnsHeader.flags & DNS_FLAG_RESPONSE) == DNS_FLAG_RESPONSE) {
        packetType = DNSPacketResponse;
    } else {
        packetType = DNSPacketRequest;
    }
}

void DNSParser::parse() {
    parseHeader();
    if (isParseFail) {
        return;
    }

    parseQuery();
    if (isParseFail) {
        return;
    }

    if (packetType == DNSPacketResponse) {
        parseAnswer();
    }
}

} // namespace logtail