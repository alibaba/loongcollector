/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "NetworkUtil.h"

#include <cstdint>

#include <array>

#if defined(__linux__)
#include <arpa/inet.h>
#endif

namespace logtail {

static const std::array<std::string, 14> TCP_STATE_STRINGS = {{"UNKNOWN_STATE",
                                                               "TCP_ESTABLISHED",
                                                               "TCP_SYN_SENT",
                                                               "TCP_SYN_RECV",
                                                               "TCP_FIN_WAIT1",
                                                               "TCP_FIN_WAIT2",
                                                               "TCP_TIME_WAIT",
                                                               "TCP_CLOSE",
                                                               "TCP_CLOSE_WAIT",
                                                               "TCP_LAST_ACK",
                                                               "TCP_LISTEN",
                                                               "TCP_CLOSING",
                                                               "TCP_NEW_SYN_RECV",
                                                               "TCP_MAX_STATES"}};

static const std::string INVALID_STATE = "INVALID_STATE";
static const std::string EMPTY_STRING = "";

static const std::string PROTOCOL_ICMP = "ICMP";
static const std::string PROTOCOL_IGMP = "IGMP";
static const std::string PROTOCOL_IP = "IP";
static const std::string PROTOCOL_TCP = "TCP";
static const std::string PROTOCOL_UDP = "UDP";
static const std::string PROTOCOL_ENCAPSULATION = "ENCAP";
static const std::string PROTOCOL_OSPF = "OSPF";
static const std::string PROTOCOL_UNKNOWN = "Unknown";

static const std::string FAMILY_INET = "AF_INET";
static const std::string FAMILY_INET6 = "AF_INET6";
static const std::string FAMILY_UNIX = "AF_UNIX";
static const std::string FAMILY_UNKNOWN = "UNKNOWN_FAMILY";

const std::string& GetStateString(uint16_t state) {
    if (state >= TCP_STATE_STRINGS.size()) {
        return INVALID_STATE;
    }
    return TCP_STATE_STRINGS[state];
}

std::string GetAddrString(uint32_t ad) {
#if defined(__linux__)
    auto addr = ntohl(ad);
    struct in_addr ip_addr;
    ip_addr.s_addr = htonl(addr);
    char ip_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &ip_addr, ip_str, INET_ADDRSTRLEN)) {
        return ip_str;
    }
#endif
    return EMPTY_STRING;
}

const std::string& GetFamilyString(uint16_t family) {
#if defined(__linux__)
    switch (family) {
        case AF_INET:
            return FAMILY_INET;
        case AF_INET6:
            return FAMILY_INET6;
        case AF_UNIX:
            return FAMILY_UNIX;
        default:
            return FAMILY_UNKNOWN;
    }
#else
    return EMPTY_STRING;
#endif
}

const std::string& GetProtocolString(uint16_t protocol) {
    switch (protocol) {
        case 1:
            return PROTOCOL_ICMP;
        case 2:
            return PROTOCOL_IGMP;
        case 4:
            return PROTOCOL_IP;
        case 6:
            return PROTOCOL_TCP;
        case 17:
            return PROTOCOL_UDP;
        case 41:
            return PROTOCOL_ENCAPSULATION;
        case 89:
            return PROTOCOL_OSPF;
        default:
            return PROTOCOL_UNKNOWN;
    }
}

} // namespace logtail
