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

#pragma once

#include <cstdint>

#include <array>
#include <string>
#include <variant>

namespace logtail {

std::string GetAddrString(uint32_t addr);
const std::string& GetFamilyString(uint16_t family);
const std::string& GetProtocolString(uint16_t protocol);
const std::string& GetStateString(uint16_t state);

enum class InetAddrFamily { kUnspecified, kIPv4, kIPv6 };

struct InetAddr {
    InetAddrFamily mFamily = InetAddrFamily::kUnspecified;
    std::variant<uint32_t, std::array<uint8_t, 16>> mIp;

    std::string AddrStr() const;
    bool IsLoopback() const;
};

struct CIDR {
    size_t mPrefixLength = 0;
    InetAddr mAddr;
};

bool CIDRContainsForIPV4(uint32_t cidrIp, size_t prefixLen, uint32_t ip);
bool ParseCIDR(const std::string& cidrStr, CIDR* cidr);

} // namespace logtail
