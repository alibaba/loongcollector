// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

extern "C" {
#include <bpf/libbpf.h>
#include <coolbpf/coolbpf.h>
};

#include <unistd.h>

#include <string>
#include <vector>

#include "BPFMapTraits.h"
#include "CallName.h"
#include "IdAllocator.h"
#include "Log.h"
#include "NetworkFilter.h"
#include "eBPFWrapper.h"
#include "ebpf/include/export.h"

namespace logtail {
namespace ebpf {

std::pair<std::string, int> ParseIpString(const std::string& addr) {
    // split
    auto del = addr.find('/');
    std::string ipStr;
    uint32_t mask = 0;
    if (del != std::string::npos) {
        ipStr = addr.substr(0, del);
        std::string maskStr = addr.substr(del + 1);
        mask = std::stoi(maskStr);
    } else {
        ipStr = addr;
    }
    return std::make_pair<std::string, int>(std::move(ipStr), mask);
}

bool ConvertIPv4StrToBytes(const std::string& ipv4Str, uint32_t& v4addr) {
    struct in_addr addr {};

    if (inet_pton(AF_INET, ipv4Str.c_str(), &addr) != 1) {
        return false;
    }
    v4addr = addr.s_addr;
    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
             "[ConvertIPv4StrToBytes] ipv4 str:%s, addr:%u, addr.s_addr:%u \n",
             ipv4Str.c_str(),
             v4addr,
             addr.s_addr);
    return true;
}

bool ConvertIPv6StrToBytes(const std::string& ipv6Str, uint8_t* ipv6Bytes) {
    if (inet_pton(AF_INET6, ipv6Str.c_str(), ipv6Bytes) != 1) {
        return false;
    }
    for (size_t i = 0; i < 4; ++i) {
        uint32_t part = *(reinterpret_cast<uint32_t*>(ipv6Bytes + i * 4));
        part = ntohl(part);
        *(reinterpret_cast<uint32_t*>(ipv6Bytes + i * 4)) = part;
    }
    return true;
}

#define IPV4_FMT "%d.%d.%d.%d"
#define IPV4_ARGS(addr) ((addr) >> 24) & 0xFF, ((addr) >> 16) & 0xFF, ((addr) >> 8) & 0xFF, (addr) & 0xFF
#define IPV6_FMT "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x"
#define IPV6_ARGS(addr) \
    ((addr)[0] >> 16) & 0xFFFF, (addr)[0] & 0xFFFF, ((addr)[1] >> 16) & 0xFFFF, (addr)[1] & 0xFFFF, \
        ((addr)[2] >> 16) & 0xFFFF, (addr)[2] & 0xFFFF, ((addr)[3] >> 16) & 0xFFFF, (addr)[3] & 0xFFFF

int SetSaddrFilter(std::shared_ptr<BPFWrapper<security_bpf>>& wrapper,
                   int /*callNameIdx*/,
                   selector_filters& filters,
                   const SecurityNetworkFilter* config) {
    int ret = 0;
    if (config->mSourceAddrList.size()) {
        selector_filter kFilter{};
        kFilter.filter_type = FILTER_TYPE_SADDR;
        kFilter.op_type = OP_TYPE_IN;
        std::vector<addr4_lpm_trie> addr4Tries;
        std::vector<addr6_lpm_trie> addr6Tries;
        for (const auto& addr : config->mSourceAddrList) {
            auto result = ParseIpString(addr);
            std::string ip = result.first;
            int mask = result.second;
            addr4_lpm_trie arg4{};
            addr6_lpm_trie arg6{};

            bool yes = ConvertIPv4StrToBytes(ip, arg4.addr);
            if (yes) {
                if (mask == 0) {
                    arg4.prefix = 32;
                } else {
                    arg4.prefix = mask;
                }
                addr4Tries.push_back(arg4);
                continue;
            }
            yes = ConvertIPv6StrToBytes(ip, (uint8_t*)&arg6.addr);
            if (yes) {
                if (mask == 0) {
                    arg4.prefix = 128;
                } else {
                    arg4.prefix = mask;
                }
                addr6Tries.push_back(arg6);
            }
        }

        if (addr4Tries.size()) {
            int ipv4Idx = IdAllocator::GetInstance()->GetNextId<Addr4Map>();
            kFilter.map_idx[0] = ipv4Idx;
            for (auto arg4 : addr4Tries) {
                uint8_t val = 1;
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "[before update] ipv4 prefix trie addr: " IPV4_FMT " mask: %u\n",
                         IPV4_ARGS(arg4.addr),
                         arg4.prefix);
                ret = wrapper->UpdateInnerMapElem<Addr4Map>(std::string("addr4lpm_maps"), &ipv4Idx, &arg4, &val, 0);
                if (ret) {
                    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "[update failed] ipv4 prefix trie data failed! addr: " IPV4_FMT " mask: %u\n",
                             IPV4_ARGS(arg4.addr),
                             arg4.prefix);
                    continue;
                }
            }
        } else {
            kFilter.map_idx[0] = -1;
        }

        if (addr6Tries.size()) {
            int ipv6Idx = IdAllocator::GetInstance()->GetNextId<Addr6Map>();
            kFilter.map_idx[1] = ipv6Idx;
            for (auto arg6 : addr6Tries) {
                uint8_t val = 1;
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "[before update] ipv6 prefix trie addr: " IPV6_FMT "mask: %u\n",
                         IPV6_ARGS(arg6.addr),
                         arg6.prefix);
                ret = wrapper->UpdateInnerMapElem<Addr6Map>(std::string("addr6lpm_maps"), &ipv6Idx, &arg6, &val, 0);
                if (ret) {
                    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "[update failed] ipv6 prefix trie data failed! addr: " IPV6_FMT " mask: %u\n",
                             IPV6_ARGS(arg6.addr),
                             arg6.prefix);
                    continue;
                }
            }
        } else {
            kFilter.map_idx[1] = -1;
        }
        filters.filters[filters.filter_count++] = kFilter;
    }

    return ret;
}

int SetSaddrBlackFilter(std::shared_ptr<BPFWrapper<security_bpf>>& wrapper,
                        int /*callNameIdx*/,
                        selector_filters& filters,
                        const SecurityNetworkFilter* config) {
    int ret = 0;
    if (config->mSourceAddrBlackList.size()) {
        selector_filter kFilter{};
        kFilter.filter_type = FILTER_TYPE_SADDR;
        kFilter.op_type = OP_TYPE_NOT_IN;
        std::vector<addr4_lpm_trie> addr4Tries;
        std::vector<addr6_lpm_trie> addr6Tries;
        for (const auto& addr : config->mSourceAddrBlackList) {
            auto result = ParseIpString(addr);
            std::string ip = result.first;
            int mask = result.second;
            addr4_lpm_trie arg4{};
            addr6_lpm_trie arg6{};

            bool yes = ConvertIPv4StrToBytes(ip, arg4.addr);
            if (yes) {
                if (mask == 0) {
                    arg4.prefix = 32;
                } else {
                    arg4.prefix = mask;
                }
                addr4Tries.push_back(arg4);
                continue;
            }
            yes = ConvertIPv6StrToBytes(ip, (uint8_t*)&arg6.addr);
            if (yes) {
                if (mask == 0) {
                    arg4.prefix = 128;
                } else {
                    arg4.prefix = mask;
                }
                addr6Tries.push_back(arg6);
            }
        }

        if (addr4Tries.size()) {
            int ipv4Idx = IdAllocator::GetInstance()->GetNextId<Addr4Map>();
            kFilter.map_idx[0] = ipv4Idx;
            for (auto arg4 : addr4Tries) {
                uint8_t val = 0;
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "[before update] ipv4 prefix trie addr: " IPV4_FMT " mask: %u\n",
                         IPV4_ARGS(arg4.addr),
                         arg4.prefix);
                ret = wrapper->UpdateInnerMapElem<Addr4Map>(std::string("addr4lpm_maps"), &ipv4Idx, &arg4, &val, 0);
                if (ret) {
                    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "[update failed] ipv4 prefix trie data failed! addr: " IPV4_FMT " mask: %u\n",
                             IPV4_ARGS(arg4.addr),
                             arg4.prefix);
                    continue;
                }
            }
        } else {
            kFilter.map_idx[0] = -1;
        }

        if (addr6Tries.size()) {
            int ipv6Idx = IdAllocator::GetInstance()->GetNextId<Addr6Map>();
            kFilter.map_idx[1] = ipv6Idx;
            for (auto arg6 : addr6Tries) {
                uint8_t val = 0;
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "[before update] ipv6 prefix trie addr: " IPV6_FMT " mask: %u\n",
                         IPV6_ARGS(arg6.addr),
                         arg6.prefix);
                ret = wrapper->UpdateInnerMapElem<Addr6Map>(std::string("addr6lpm_maps"), &ipv6Idx, &arg6, &val, 0);
                if (ret) {
                    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "[update failed] ipv6 prefix trie data failed! addr: " IPV6_FMT " mask: %u\n",
                             IPV6_ARGS(arg6.addr),
                             arg6.prefix);
                    continue;
                }
            }
        } else {
            kFilter.map_idx[1] = -1;
        }
        filters.filters[filters.filter_count++] = kFilter;
    }

    return ret;
}

int SetDaddrFilter(std::shared_ptr<BPFWrapper<security_bpf>>& wrapper,
                   int /*callNameIdx*/,
                   selector_filters& filters,
                   const SecurityNetworkFilter* config) {
    int ret = 0;
    if (config->mDestAddrList.size()) {
        selector_filter kFilter{};
        kFilter.filter_type = FILTER_TYPE_DADDR;
        kFilter.op_type = OP_TYPE_IN;
        std::vector<addr4_lpm_trie> addr4Tries;
        std::vector<addr6_lpm_trie> addr6Tries;
        for (const auto& addr : config->mDestAddrList) {
            auto result = ParseIpString(addr);
            std::string ip = result.first;
            int mask = result.second;
            addr4_lpm_trie arg4{};
            addr6_lpm_trie arg6{};
            ::memset(&arg4, 0, sizeof(arg4));
            ::memset(&arg6, 0, sizeof(arg6));
            bool yes = ConvertIPv4StrToBytes(ip, arg4.addr);
            if (yes) {
                if (mask == 0) {
                    arg4.prefix = 32;
                } else {
                    arg4.prefix = mask;
                }
                addr4Tries.push_back(arg4);
                continue;
            }
            yes = ConvertIPv6StrToBytes(ip, (uint8_t*)&arg6.addr);
            if (yes) {
                if (mask == 0) {
                    arg4.prefix = 128;
                } else {
                    arg4.prefix = mask;
                }
                addr6Tries.push_back(arg6);
            }
        }

        if (addr4Tries.size()) {
            int ipv4Idx = IdAllocator::GetInstance()->GetNextId<Addr4Map>();
            kFilter.map_idx[0] = ipv4Idx;
            for (auto arg4 : addr4Tries) {
                uint8_t val = 1;
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "[before update] ipv4 prefix trie addr: " IPV4_FMT " mask: %u inner map idx: %d\n",
                         IPV4_ARGS(arg4.addr),
                         arg4.prefix,
                         ipv4Idx);
                ret = wrapper->UpdateInnerMapElem<Addr4Map>(std::string("addr4lpm_maps"), &ipv4Idx, &arg4, &val, 0);
                if (ret) {
                    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "[update failed] ipv4 prefix trie data failed! addr: " IPV4_FMT " mask: %u\n",
                             IPV4_ARGS(arg4.addr),
                             arg4.prefix);
                    continue;
                }
            }
        } else {
            kFilter.map_idx[0] = -1;
        }

        if (addr6Tries.size()) {
            int ipv6Idx = IdAllocator::GetInstance()->GetNextId<Addr6Map>();
            kFilter.map_idx[1] = ipv6Idx;
            for (auto arg6 : addr6Tries) {
                uint8_t val = 1;
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "[before update] ipv6 prefix trie addr: " IPV6_FMT " mask: %u\n",
                         IPV6_ARGS(arg6.addr),
                         arg6.prefix);
                ret = wrapper->UpdateInnerMapElem<Addr6Map>(std::string("addr6lpm_maps"), &ipv6Idx, &arg6, &val, 0);
                if (ret) {
                    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "[update failed] ipv6 prefix trie data failed! addr: " IPV6_FMT " mask: %u\n",
                             IPV6_ARGS(arg6.addr),
                             arg6.prefix);
                    continue;
                }
            }
        } else {
            kFilter.map_idx[1] = -1;
        }
        filters.filters[filters.filter_count++] = kFilter;
    }

    return ret;
}

int SetDaddrBlackFilter(std::shared_ptr<BPFWrapper<security_bpf>>& wrapper,
                        int /*callNameIdx*/,
                        selector_filters& filters,
                        const SecurityNetworkFilter* config) {
    int ret = 0;
    if (config->mDestAddrBlackList.size()) {
        selector_filter kFilter{};
        kFilter.filter_type = FILTER_TYPE_DADDR;
        kFilter.op_type = OP_TYPE_NOT_IN;
        std::vector<addr4_lpm_trie> addr4Tries;
        std::vector<addr6_lpm_trie> addr6Tries;
        for (const auto& addr : config->mDestAddrBlackList) {
            auto result = ParseIpString(addr);
            std::string ip = result.first;
            int mask = result.second;
            addr4_lpm_trie arg4{};
            addr6_lpm_trie arg6{};
            bool yes = ConvertIPv4StrToBytes(ip, arg4.addr);
            if (yes) {
                if (mask == 0) {
                    arg4.prefix = 32;
                } else {
                    arg4.prefix = mask;
                }
                addr4Tries.push_back(arg4);
                continue;
            }
            yes = ConvertIPv6StrToBytes(ip, (uint8_t*)&arg6.addr);
            if (yes) {
                if (mask == 0) {
                    arg4.prefix = 128;
                } else {
                    arg4.prefix = mask;
                }
                addr6Tries.push_back(arg6);
            }
        }

        if (addr4Tries.size()) {
            int ipv4Idx = IdAllocator::GetInstance()->GetNextId<Addr4Map>();
            kFilter.map_idx[0] = ipv4Idx;
            for (auto arg4 : addr4Tries) {
                uint8_t val = 1;
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "[before update] ipv4 prefix trie addr: " IPV4_FMT " mask: %u inner map idx: %d\n",
                         IPV4_ARGS(arg4.addr),
                         arg4.prefix,
                         ipv4Idx);
                ret = wrapper->UpdateInnerMapElem<Addr4Map>(std::string("addr4lpm_maps"), &ipv4Idx, &arg4, &val, 0);
                if (ret) {
                    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "[update failed] ipv4 prefix trie data failed! addr: " IPV4_FMT " mask: %u\n",
                             IPV4_ARGS(arg4.addr),
                             arg4.prefix);
                    continue;
                }
            }
        } else {
            kFilter.map_idx[0] = -1;
        }

        if (addr6Tries.size()) {
            int ipv6Idx = IdAllocator::GetInstance()->GetNextId<Addr6Map>();
            kFilter.map_idx[1] = ipv6Idx;
            for (auto arg6 : addr6Tries) {
                uint8_t val = 1;
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "[before update] ipv6 prefix trie addr: " IPV6_FMT " mask: %u\n",
                         IPV6_ARGS(arg6.addr),
                         arg6.prefix);
                ret = wrapper->UpdateInnerMapElem<Addr6Map>(std::string("addr6lpm_maps"), &ipv6Idx, &arg6, &val, 0);
                if (ret) {
                    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "[update failed] ipv6 prefix trie data failed! addr: " IPV6_FMT " mask: %u\n",
                             IPV6_ARGS(arg6.addr),
                             arg6.prefix);
                    continue;
                }
            }
        } else {
            kFilter.map_idx[1] = -1;
        }
        filters.filters[filters.filter_count++] = kFilter;
    }

    return ret;
}

int SetSportFilter(std::shared_ptr<BPFWrapper<security_bpf>>& wrapper,
                   int /*callNameIdx*/,
                   selector_filters& filters,
                   const SecurityNetworkFilter* config) {
    int idx = IdAllocator::GetInstance()->GetNextId<PortMap>();
    selector_filter kFilter{};
    kFilter.filter_type = FILTER_TYPE_SPORT;
    kFilter.op_type = OP_TYPE_IN;
    kFilter.map_idx[0] = idx;
    if (filters.filter_count >= MAX_FILTER_FOR_PER_CALLNAME) {
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN, "filters count exceeded! max: %d\n", MAX_FILTER_FOR_PER_CALLNAME);
        return 1;
    }
    filters.filters[filters.filter_count++] = kFilter;

    if (config->mSourcePortList.empty()) {
        return 0;
    }
    for (uint32_t port : config->mSourcePortList) {
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO, "begin to update map in map for filter detail, idx: %d\n", idx);
        uint8_t val = 1;
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO, "[before update][white] source port: %u\n", port);
        int ret = wrapper->UpdateInnerMapElem<PortMap>(std::string("port_maps"), &idx, &port, &val, 0);
        if (ret) {
            ebpf_log(
                eBPFLogType::NAMI_LOG_TYPE_WARN, "[update failed][white] port map update failed! sport: %u\n", port);
            continue;
        }
    }
    return 0;
}

int SetSportBlackFilter(std::shared_ptr<BPFWrapper<security_bpf>>& wrapper,
                        int /*callNameIdx*/,
                        selector_filters& filters,
                        const SecurityNetworkFilter* config) {
    int idx = IdAllocator::GetInstance()->GetNextId<PortMap>();
    selector_filter kFilter{};
    kFilter.filter_type = FILTER_TYPE_SPORT;
    kFilter.op_type = OP_TYPE_NOT_IN;
    kFilter.map_idx[0] = idx;
    if (filters.filter_count >= MAX_FILTER_FOR_PER_CALLNAME) {
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN, "filters count exceeded! max: %d\n", MAX_FILTER_FOR_PER_CALLNAME);
        return 1;
    }
    filters.filters[filters.filter_count++] = kFilter;

    if (config->mSourcePortBlackList.empty()) {
        return 0;
    }
    for (uint32_t port : config->mSourcePortBlackList) {
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO, "begin to update map in map for filter detail, idx: %d\n", idx);
        uint8_t val = 0;
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO, "[before update][black] source port: %u\n", port);
        int ret = wrapper->UpdateInnerMapElem<PortMap>(std::string("port_maps"), &idx, &port, &val, 0);
        if (ret) {
            ebpf_log(
                eBPFLogType::NAMI_LOG_TYPE_WARN, "[update failed][black] port map update failed! sport: %u\n", port);
            continue;
        }
    }
    return 0;
}

int SetDportFilter(std::shared_ptr<BPFWrapper<security_bpf>>& wrapper,
                   int /*callNameIdx*/,
                   selector_filters& filters,
                   const SecurityNetworkFilter* config) {
    int idx = IdAllocator::GetInstance()->GetNextId<PortMap>();
    selector_filter kFilter{};
    kFilter.filter_type = FILTER_TYPE_DPORT;
    kFilter.op_type = OP_TYPE_IN;
    kFilter.map_idx[0] = idx;
    if (filters.filter_count >= MAX_FILTER_FOR_PER_CALLNAME) {
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN, "filters count exceeded! max: %d\n", MAX_FILTER_FOR_PER_CALLNAME);
        return 1;
    }
    filters.filters[filters.filter_count++] = kFilter;

    if (config->mDestPortList.empty()) {
        return 0;
    }
    for (uint32_t port : config->mDestPortList) {
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO, "begin to update map in map for filter detail, idx: %d\n", idx);
        uint8_t val = 1;
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO, "[before update][white] dest port: %u\n", port);
        int ret = wrapper->UpdateInnerMapElem<PortMap>(std::string("port_maps"), &idx, &port, &val, 0);
        if (ret) {
            ebpf_log(
                eBPFLogType::NAMI_LOG_TYPE_WARN, "[update failed][white] port map update failed! dport: %u\n", port);
            continue;
        }
    }

    return 0;
}

int SetDportBlackFilter(std::shared_ptr<BPFWrapper<security_bpf>>& wrapper,
                        int /*callNameIdx*/,
                        selector_filters& filters,
                        const SecurityNetworkFilter* config) {
    int idx = IdAllocator::GetInstance()->GetNextId<PortMap>();
    selector_filter kFilter{};
    kFilter.filter_type = FILTER_TYPE_DPORT;
    kFilter.op_type = OP_TYPE_NOT_IN;
    kFilter.map_idx[0] = idx;
    if (filters.filter_count >= MAX_FILTER_FOR_PER_CALLNAME) {
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN, "filters count exceeded! max: %d\n", MAX_FILTER_FOR_PER_CALLNAME);
        return 1;
    }
    filters.filters[filters.filter_count++] = kFilter;

    if (config->mDestPortBlackList.empty()) {
        return 0;
    }
    for (uint32_t port : config->mDestPortBlackList) {
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO, "begin to update map in map for filter detail, idx: %d\n", idx);
        uint8_t val = 0;
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO, "[before update][black] dest port: %u\n", port);
        int ret = wrapper->UpdateInnerMapElem<PortMap>(std::string("port_maps"), &idx, &port, &val, 0);
        if (ret) {
            ebpf_log(
                eBPFLogType::NAMI_LOG_TYPE_WARN, "[update failed][black] port map update failed! dport: %u\n", port);
            continue;
        }
    }

    return 0;
}

int CreateNetworkFilterForCallname(
    std::shared_ptr<BPFWrapper<security_bpf>>& wrapper,
    const std::string& callName,
    const std::variant<std::monostate, SecurityFileFilter, SecurityNetworkFilter>& newConfig) {
    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
             "EnableCallName %s idx: %d hold: %d\n",
             callName.c_str(),
             newConfig.index(),
             std::holds_alternative<SecurityFileFilter>(newConfig));

    std::vector<AttachProgOps> attachOps = {AttachProgOps("kprobe_" + callName, true)};
    int ret = 0;
    int callNameIdx = GetCallNameIdx(callName);
    if (callNameIdx < 0) {
        return 1;
    }
    const auto* filter = std::get_if<SecurityNetworkFilter>(&newConfig);

    if (filter) {
        if (filter->mDestAddrBlackList.size() && filter->mDestAddrList.size()) {
            return 1;
        }
        if (filter->mSourceAddrBlackList.size() && filter->mSourceAddrList.size()) {
            return 1;
        }
        if (filter->mDestPortList.size() && filter->mDestPortBlackList.size()) {
            return 1;
        }
        if (filter->mSourcePortList.size() && filter->mSourcePortBlackList.size()) {
            return 1;
        }
        selector_filters kernelFilters{};
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO, "filter not empty!\n");

        if (filter->mDestAddrList.size()) {
            SetDaddrFilter(wrapper, callNameIdx, kernelFilters, filter);
        }
        if (filter->mDestAddrBlackList.size()) {
            SetDaddrBlackFilter(wrapper, callNameIdx, kernelFilters, filter);
        }
        if (filter->mSourceAddrList.size()) {
            SetSaddrFilter(wrapper, callNameIdx, kernelFilters, filter);
        }
        if (filter->mSourceAddrBlackList.size()) {
            SetSaddrBlackFilter(wrapper, callNameIdx, kernelFilters, filter);
        }
        if (filter->mDestPortList.size()) {
            SetDportFilter(wrapper, callNameIdx, kernelFilters, filter);
        }
        if (filter->mDestPortBlackList.size()) {
            SetDportBlackFilter(wrapper, callNameIdx, kernelFilters, filter);
        }
        if (filter->mSourcePortList.size()) {
            SetSportFilter(wrapper, callNameIdx, kernelFilters, filter);
        }
        if (filter->mSourcePortBlackList.size()) {
            SetSportBlackFilter(wrapper, callNameIdx, kernelFilters, filter);
        }

        wrapper->UpdateBPFHashMap("filter_map", &callNameIdx, &kernelFilters, 0);
    }

    return ret;
}

int DeleteNetworkFilterForCallname(std::shared_ptr<BPFWrapper<security_bpf>>& wrapper, const std::string& callName) {
    std::vector<AttachProgOps> attachOps = {AttachProgOps("kprobe_" + callName, true)};
    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO, "DisableCallName %s\n", callName.c_str());

    int callNameIdx = GetCallNameIdx(callName);
    if (callNameIdx < 0) {
        return 1;
    }
    int ret = 0;

    selector_filters kernelFilters{};
    ret = wrapper->LookupBPFHashMap("filter_map", &callNameIdx, &kernelFilters);
    if (ret) {
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO, "there is no filter for call name: %s\n", callName.c_str());
        return 0;
    }

    for (int i = 0; i < kernelFilters.filter_count; i++) {
        auto filter = kernelFilters.filters[i];
        switch (filter.filter_type) {
            case FILTER_TYPE_SADDR:
            case FILTER_TYPE_DADDR:
            case FILTER_TYPE_NOT_SADDR:
            case FILTER_TYPE_NOT_DADDR: {
                auto v4 = filter.map_idx[0];
                auto v6 = filter.map_idx[1];
                if (v4 != __u32(-1)) {
                    wrapper->DeleteInnerMap<Addr4Map>("addr4lpm_maps", &v4);
                    IdAllocator::GetInstance()->ReleaseId<Addr4Map>(v4);
                }
                if (v6 != __u32(-1)) {
                    wrapper->DeleteInnerMap<Addr6Map>("addr6lpm_maps", &v6);
                    IdAllocator::GetInstance()->ReleaseId<Addr6Map>(v6);
                }
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "release filter for type: %d mapIdx: [%u:%u]\n",
                         static_cast<int>(filter.filter_type),
                         v4,
                         v6);
                break;
            }
            case FILTER_TYPE_SPORT:
            case FILTER_TYPE_DPORT:
            case FILTER_TYPE_NOT_SPORT:
            case FILTER_TYPE_NOT_DPORT: {
                auto idx = filter.map_idx[0];
                wrapper->DeleteInnerMap<PortMap>("port_maps", &idx);
                IdAllocator::GetInstance()->ReleaseId<PortMap>(idx);
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "release filter for type: %d mapIdx: %u\n",
                         static_cast<int>(filter.filter_type),
                         idx);
                break;
            }
            default:
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN,
                         "abnormal, unknown filter type %d\n",
                         static_cast<int>(filter.filter_type));
        }
    }

    ::memset(&kernelFilters, 0, sizeof(kernelFilters));
    wrapper->UpdateBPFHashMap("filter_map", &callNameIdx, &kernelFilters, 0);

    return ret;
}

} // namespace ebpf
} // namespace logtail
