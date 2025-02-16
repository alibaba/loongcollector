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

#include <string>
#include <vector>

#if defined(__linux__)
#include <arpa/inet.h>
#endif

namespace logtail {

const std::vector<std::string> tcpStateString = {
    "UNKNOWN_STATE",
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
    "TCP_MAX_STATES",
};

std::string GetStateString(uint16_t state) {
    if (state >= tcpStateString.size())
        return "INVALID_STATE";
    return tcpStateString[state];
}

std::string GetAddrString(uint32_t ad) {
#if defined(__linux__)
    auto addr = ntohl(ad);
    struct in_addr ip_addr;
    ip_addr.s_addr = htonl(addr);
    char* ip_str = inet_ntoa(ip_addr);
    return std::string(ip_str);
#else
    return "";
#endif
}
std::string GetFamilyString(uint16_t family) {
#if defined(__linux__)
    if (family == AF_INET) {
        return "AF_INET";
    } else if (family == AF_INET6) {
        return "AF_INET6";
    } else if (family == AF_UNIX) {
        return "AF_UNIX";
    } else {
        return std::to_string(family);
    }
#else
    return "";
#endif
}
std::string GetProtocolString(uint16_t protocol) {
    switch (protocol) {
        case 1:
            return "ICMP";
        case 2:
            return "IGMP";
        case 4:
            return "IP";
        case 6:
            return "TCP";
        case 17:
            return "UDP";
        case 41:
            return "ENCAP";
        case 89:
            return "OSPF";
        default:
            return "Unknown";
    }
}
} // namespace logtail
