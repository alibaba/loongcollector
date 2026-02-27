/*
 * Copyright 2025 iLogtail Authors
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
#include <variant>

#include "common/StringTools.h"
#include "logger/Logger.h"

#if defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/un.h>
#elif defined(_MSC_VER)
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace logtail {

static const std::string EMPTY_STRING = "";

const std::string& GetStateString(uint16_t state) {
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
    if (state >= TCP_STATE_STRINGS.size()) {
        return INVALID_STATE;
    }
    return TCP_STATE_STRINGS[state];
}

std::string GetAddrString(uint32_t ad) {
#if defined(__linux__)
    auto addr = ntohl(ad);
    struct in_addr ipAddr;
    ipAddr.s_addr = htonl(addr);
    char ip_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &ipAddr, ip_str, INET_ADDRSTRLEN)) {
        return ip_str;
    }
#endif
    return EMPTY_STRING;
}

const std::string& GetFamilyString(uint16_t family) {
#if defined(__linux__)
    static const std::string FAMILY_INET = "AF_INET";
    static const std::string FAMILY_INET6 = "AF_INET6";
    static const std::string FAMILY_UNIX = "AF_UNIX";
    static const std::string FAMILY_UNKNOWN = "UNKNOWN_FAMILY";
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
    static const std::string PROTOCOL_ICMP = "ICMP";
    static const std::string PROTOCOL_IGMP = "IGMP";
    static const std::string PROTOCOL_IP = "IP";
    static const std::string PROTOCOL_TCP = "TCP";
    static const std::string PROTOCOL_UDP = "UDP";
    static const std::string PROTOCOL_ENCAPSULATION = "ENCAP";
    static const std::string PROTOCOL_OSPF = "OSPF";
    static const std::string PROTOCOL_UNKNOWN = "Unknown";
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

constexpr int kIPv4BitLen = 32;
constexpr int kIPv6BitLen = 128;

bool CIDRContainsForIPV4(uint32_t cidrIp, size_t prefixLen, uint32_t ip) {
    LOG_DEBUG(sLogger, ("cidr ip", cidrIp)("ip", ip));
    return ntohl(cidrIp) >> (kIPv4BitLen - prefixLen) == ntohl(ip) >> (kIPv4BitLen - prefixLen);
}

// The IPv4 IP is located in the last 32-bit word of IPv6 address.
constexpr int kIPv4Offset = 3;

bool ParseIPv4Addr(const std::string& addrStr, struct in_addr* inAddr) {
    if (!inet_pton(AF_INET, addrStr.c_str(), inAddr)) {
        return false;
    }
    return true;
}

bool ParseIPv6Addr(const std::string& addrStr, struct in6_addr* in6Addr) {
    if (!inet_pton(AF_INET6, addrStr.c_str(), in6Addr)) {
        return false;
    }
    return true;
}

bool ParseIPAddr(const std::string& addrStr, InetAddr* ipAddr) {
    struct in_addr v4Addr = {};
    struct in6_addr v6Addr = {};
    v6Addr.s6_addr;

    if (ParseIPv4Addr(addrStr, &v4Addr)) {
        ipAddr->mFamily = InetAddrFamily::kIPv4;
        ipAddr->mIp = v4Addr.s_addr;
    } else if (ParseIPv6Addr(addrStr, &v6Addr)) {
        ipAddr->mFamily = InetAddrFamily::kIPv6;
        ipAddr->mIp = std::array<uint8_t, 16>();
        std::copy(std::begin(v6Addr.s6_addr),
                  std::end(v6Addr.s6_addr),
                  std::get<std::array<uint8_t, 16>>(ipAddr->mIp).begin());
    } else {
        return false;
    }

    return true;
}

bool ParseCIDR(const std::string& cidrStr, CIDR* cidr) {
    auto items = StringSpliter(cidrStr, "/");
    if (items.size() != 2) {
        return false;
    }
    int prefixLen = -1;
    StringTo(items[1], prefixLen);
    if (prefixLen < 0) {
        return false;
    }
    InetAddr addr;
    ParseIPAddr(items[0], &addr);

    if (addr.mFamily == InetAddrFamily::kIPv4 && prefixLen > kIPv4BitLen) {
        return false;
    }
    if (addr.mFamily == InetAddrFamily::kIPv6 && prefixLen > kIPv6BitLen) {
        return false;
    }

    cidr->mAddr = std::move(addr);
    cidr->mPrefixLength = prefixLen;
    return true;
}

} // namespace logtail
