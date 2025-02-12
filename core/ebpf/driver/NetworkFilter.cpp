extern "C" {
#include <bpf/libbpf.h>
#include <coolbpf/coolbpf.h>
};

#include <coolbpf/security.skel.h>
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
    auto del = addr.find("/");
    std::string ip_str = "";
    uint32_t mask = 0;
    if (del != std::string::npos) {
        ip_str = addr.substr(0, del);
        std::string mask_str = addr.substr(del + 1);
        mask = std::stoi(mask_str);
    } else {
        ip_str = addr;
    }
    return std::make_pair<std::string, int>(std::move(ip_str), mask);
}

bool ConvertIPv4StrToBytes(const std::string& ipv4Str, uint32_t& v4addr) {
    struct in_addr addr;

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

int SetSaddrFilter(std::shared_ptr<BPFWrapper<security_bpf>> wrapper,
                   int call_name_idx,
                   selector_filters& filters,
                   const SecurityNetworkFilter* config) {
    int ret = 0;
    if (config->mSourceAddrList.size()) {
        selector_filter k_filter;
        ::memset(&k_filter, 0, sizeof(k_filter));
        k_filter.filter_type = FILTER_TYPE_SADDR;
        k_filter.op_type = OP_TYPE_IN;
        filters.filters[filters.filter_count++] = k_filter;
        std::vector<addr4_lpm_trie> addr4_tries;
        std::vector<addr6_lpm_trie> addr6_tries;
        for (auto& addr : config->mSourceAddrList) {
            auto result = ParseIpString(addr);
            std::string ip = result.first;
            int mask = result.second;
            addr4_lpm_trie arg4;
            addr6_lpm_trie arg6;
            ::memset(&arg4, 0, sizeof(arg4));
            ::memset(&arg6, 0, sizeof(arg6));

            bool yes = ConvertIPv4StrToBytes(ip, arg4.addr);
            if (yes) {
                if (mask == 0)
                    arg4.prefix = 32;
                else
                    arg4.prefix = mask;
                addr4_tries.push_back(arg4);
                continue;
            }
            yes = ConvertIPv6StrToBytes(ip, (uint8_t*)&arg6.addr);
            if (yes) {
                if (mask == 0)
                    arg4.prefix = 128;
                else
                    arg4.prefix = mask;
                addr6_tries.push_back(arg6);
            }
        }

        if (addr4_tries.size()) {
            int ipv4_idx = IdAllocator::GetInstance()->GetNextId<Addr4Map>();
            k_filter.map_idx[0] = ipv4_idx;
            for (auto arg4 : addr4_tries) {
                uint8_t val = 1;
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "[before update] ipv4 prefix trie addr: %d mask: %u\n",
                         arg4.addr,
                         arg4.prefix);
                ret = wrapper->UpdateInnerMapElem<Addr4Map>(std::string("addr4lpm_maps"), &ipv4_idx, &arg4, &val, 0);
                if (ret) {
                    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "[update failed] ipv4 prefix trie data failed! addr: %s mask: %u\n",
                             arg4.addr,
                             arg4.prefix);
                    continue;
                }
            }
        } else {
            k_filter.map_idx[0] = -1;
        }

        if (addr6_tries.size()) {
            int ipv6_idx = IdAllocator::GetInstance()->GetNextId<Addr6Map>();
            k_filter.map_idx[1] = ipv6_idx;
            for (auto arg6 : addr6_tries) {
                uint8_t val = 1;
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "[before update] ipv6 prefix trie addr: %d mask: %u\n",
                         arg6.addr,
                         arg6.prefix);
                ret = wrapper->UpdateInnerMapElem<Addr6Map>(std::string("addr6lpm_maps"), &ipv6_idx, &arg6, &val, 0);
                if (ret) {
                    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "[update failed] ipv6 prefix trie data failed! addr: %s mask: %u\n",
                             arg6.addr,
                             arg6.prefix);
                    continue;
                }
            }
        } else {
            k_filter.map_idx[1] = -1;
        }
        filters.filters[filters.filter_count++] = k_filter;
    }

    return ret;
}

int SetSaddrBlackFilter(std::shared_ptr<BPFWrapper<security_bpf>> wrapper,
                        int call_name_idx,
                        selector_filters& filters,
                        const SecurityNetworkFilter* config) {
    int ret = 0;
    if (config->mSourceAddrBlackList.size()) {
        selector_filter k_filter;
        ::memset(&k_filter, 0, sizeof(k_filter));
        k_filter.filter_type = FILTER_TYPE_SADDR;
        k_filter.op_type = OP_TYPE_NOT_IN;
        filters.filters[filters.filter_count++] = k_filter;
        std::vector<addr4_lpm_trie> addr4_tries;
        std::vector<addr6_lpm_trie> addr6_tries;
        for (auto& addr : config->mSourceAddrBlackList) {
            auto result = ParseIpString(addr);
            std::string ip = result.first;
            int mask = result.second;
            addr4_lpm_trie arg4;
            addr6_lpm_trie arg6;
            ::memset(&arg4, 0, sizeof(arg4));
            ::memset(&arg6, 0, sizeof(arg6));

            bool yes = ConvertIPv4StrToBytes(ip, arg4.addr);
            if (yes) {
                if (mask == 0)
                    arg4.prefix = 32;
                else
                    arg4.prefix = mask;
                addr4_tries.push_back(arg4);
                continue;
            }
            yes = ConvertIPv6StrToBytes(ip, (uint8_t*)&arg6.addr);
            if (yes) {
                if (mask == 0)
                    arg4.prefix = 128;
                else
                    arg4.prefix = mask;
                addr6_tries.push_back(arg6);
            }
        }

        if (addr4_tries.size()) {
            int ipv4_idx = IdAllocator::GetInstance()->GetNextId<Addr4Map>();
            k_filter.map_idx[0] = ipv4_idx;
            for (auto arg4 : addr4_tries) {
                uint8_t val = 0;
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "[before update] ipv4 prefix trie addr: %d mask: %u\n",
                         arg4.addr,
                         arg4.prefix);
                ret = wrapper->UpdateInnerMapElem<Addr4Map>(std::string("addr4lpm_maps"), &ipv4_idx, &arg4, &val, 0);
                if (ret) {
                    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "[update failed] ipv4 prefix trie data failed! addr: %s mask: %u\n",
                             arg4.addr,
                             arg4.prefix);
                    continue;
                }
            }
        } else {
            k_filter.map_idx[0] = -1;
        }

        if (addr6_tries.size()) {
            int ipv6_idx = IdAllocator::GetInstance()->GetNextId<Addr6Map>();
            k_filter.map_idx[1] = ipv6_idx;
            for (auto arg6 : addr6_tries) {
                uint8_t val = 0;
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "[before update] ipv6 prefix trie addr: %d mask: %u\n",
                         arg6.addr,
                         arg6.prefix);
                ret = wrapper->UpdateInnerMapElem<Addr6Map>(std::string("addr6lpm_maps"), &ipv6_idx, &arg6, &val, 0);
                if (ret) {
                    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "[update failed] ipv6 prefix trie data failed! addr: %s mask: %u\n",
                             arg6.addr,
                             arg6.prefix);
                    continue;
                }
            }
        } else {
            k_filter.map_idx[1] = -1;
        }
        filters.filters[filters.filter_count++] = k_filter;
    }

    return ret;
}

int SetDaddrFilter(std::shared_ptr<BPFWrapper<security_bpf>> wrapper,
                   int call_name_idx,
                   selector_filters& filters,
                   const SecurityNetworkFilter* config) {
    int ret = 0;
    if (config->mDestAddrList.size()) {
        selector_filter k_filter;
        ::memset(&k_filter, 0, sizeof(k_filter));
        k_filter.filter_type = FILTER_TYPE_DADDR;
        k_filter.op_type = OP_TYPE_IN;
        filters.filters[filters.filter_count++] = k_filter;
        std::vector<addr4_lpm_trie> addr4_tries;
        std::vector<addr6_lpm_trie> addr6_tries;
        for (auto& addr : config->mDestAddrList) {
            auto result = ParseIpString(addr);
            std::string ip = result.first;
            int mask = result.second;
            addr4_lpm_trie arg4;
            addr6_lpm_trie arg6;
            ::memset(&arg4, 0, sizeof(arg4));
            ::memset(&arg6, 0, sizeof(arg6));
            bool yes = ConvertIPv4StrToBytes(ip, arg4.addr);
            if (yes) {
                if (mask == 0)
                    arg4.prefix = 32;
                else
                    arg4.prefix = mask;
                addr4_tries.push_back(arg4);
                continue;
            }
            yes = ConvertIPv6StrToBytes(ip, (uint8_t*)&arg6.addr);
            if (yes) {
                if (mask == 0)
                    arg4.prefix = 128;
                else
                    arg4.prefix = mask;
                addr6_tries.push_back(arg6);
            }
        }

        if (addr4_tries.size()) {
            int ipv4_idx = IdAllocator::GetInstance()->GetNextId<Addr4Map>();
            k_filter.map_idx[0] = ipv4_idx;
            for (auto arg4 : addr4_tries) {
                uint8_t val = 1;
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "[before update] ipv4 prefix trie addr: %d mask: %u inner map idx: %d\n",
                         arg4.addr,
                         arg4.prefix,
                         ipv4_idx);
                ret = wrapper->UpdateInnerMapElem<Addr4Map>(std::string("addr4lpm_maps"), &ipv4_idx, &arg4, &val, 0);
                if (ret) {
                    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "[update failed] ipv4 prefix trie data failed! addr: %s mask: %u\n",
                             arg4.addr,
                             arg4.prefix);
                    continue;
                }
            }
        } else {
            k_filter.map_idx[0] = -1;
        }

        if (addr6_tries.size()) {
            int ipv6_idx = IdAllocator::GetInstance()->GetNextId<Addr6Map>();
            k_filter.map_idx[1] = ipv6_idx;
            for (auto arg6 : addr6_tries) {
                uint8_t val = 1;
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "[before update] ipv6 prefix trie addr: %d mask: %u\n",
                         arg6.addr,
                         arg6.prefix);
                ret = wrapper->UpdateInnerMapElem<Addr6Map>(std::string("addr6lpm_maps"), &ipv6_idx, &arg6, &val, 0);
                if (ret) {
                    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "[update failed] ipv6 prefix trie data failed! addr: %s mask: %u\n",
                             arg6.addr,
                             arg6.prefix);
                    continue;
                }
            }
        } else {
            k_filter.map_idx[1] = -1;
        }
        filters.filters[filters.filter_count++] = k_filter;
    }

    return ret;
}

int SetDaddrBlackFilter(std::shared_ptr<BPFWrapper<security_bpf>> wrapper,
                        int call_name_idx,
                        selector_filters& filters,
                        const SecurityNetworkFilter* config) {
    int ret = 0;
    if (config->mDestAddrBlackList.size()) {
        selector_filter k_filter;
        ::memset(&k_filter, 0, sizeof(k_filter));
        k_filter.filter_type = FILTER_TYPE_DADDR;
        k_filter.op_type = OP_TYPE_NOT_IN;
        filters.filters[filters.filter_count++] = k_filter;
        std::vector<addr4_lpm_trie> addr4_tries;
        std::vector<addr6_lpm_trie> addr6_tries;
        for (auto& addr : config->mDestAddrBlackList) {
            auto result = ParseIpString(addr);
            std::string ip = result.first;
            int mask = result.second;
            addr4_lpm_trie arg4;
            addr6_lpm_trie arg6;
            ::memset(&arg4, 0, sizeof(arg4));
            ::memset(&arg6, 0, sizeof(arg6));
            bool yes = ConvertIPv4StrToBytes(ip, arg4.addr);
            if (yes) {
                if (mask == 0)
                    arg4.prefix = 32;
                else
                    arg4.prefix = mask;
                addr4_tries.push_back(arg4);
                continue;
            }
            yes = ConvertIPv6StrToBytes(ip, (uint8_t*)&arg6.addr);
            if (yes) {
                if (mask == 0)
                    arg4.prefix = 128;
                else
                    arg4.prefix = mask;
                addr6_tries.push_back(arg6);
            }
        }

        if (addr4_tries.size()) {
            int ipv4_idx = IdAllocator::GetInstance()->GetNextId<Addr4Map>();
            k_filter.map_idx[0] = ipv4_idx;
            for (auto arg4 : addr4_tries) {
                uint8_t val = 1;
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "[before update] ipv4 prefix trie addr: %d mask: %u inner map idx: %d\n",
                         arg4.addr,
                         arg4.prefix,
                         ipv4_idx);
                ret = wrapper->UpdateInnerMapElem<Addr4Map>(std::string("addr4lpm_maps"), &ipv4_idx, &arg4, &val, 0);
                if (ret) {
                    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "[update failed] ipv4 prefix trie data failed! addr: %u mask: %u\n",
                             arg4.addr,
                             arg4.prefix);
                    continue;
                }
            }
        } else {
            k_filter.map_idx[0] = -1;
        }

        if (addr6_tries.size()) {
            int ipv6_idx = IdAllocator::GetInstance()->GetNextId<Addr6Map>();
            k_filter.map_idx[1] = ipv6_idx;
            for (auto arg6 : addr6_tries) {
                uint8_t val = 1;
                ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
                         "[before update] ipv6 prefix trie addr: %d mask: %u\n",
                         arg6.addr,
                         arg6.prefix);
                ret = wrapper->UpdateInnerMapElem<Addr6Map>(std::string("addr6lpm_maps"), &ipv6_idx, &arg6, &val, 0);
                if (ret) {
                    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "[update failed] ipv6 prefix trie data failed! addr: %s mask: %u\n",
                             arg6.addr,
                             arg6.prefix);
                    continue;
                }
            }
        } else {
            k_filter.map_idx[1] = -1;
        }
        filters.filters[filters.filter_count++] = k_filter;
    }

    return ret;
}

int SetSportFilter(std::shared_ptr<BPFWrapper<security_bpf>> wrapper,
                   int call_name_idx,
                   selector_filters& filters,
                   const SecurityNetworkFilter* config) {
    int idx = IdAllocator::GetInstance()->GetNextId<PortMap>();
    selector_filter k_filter;
    ::memset(&k_filter, 0, sizeof(k_filter));
    k_filter.filter_type = FILTER_TYPE_SPORT;
    k_filter.op_type = OP_TYPE_IN;
    k_filter.map_idx[0] = idx;
    if (filters.filter_count >= MAX_FILTER_FOR_PER_CALLNAME) {
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN, "filters count exceeded! max: %d\n", MAX_FILTER_FOR_PER_CALLNAME);
        return 1;
    }
    filters.filters[filters.filter_count++] = k_filter;

    if (config->mSourcePortList.empty())
        return 0;
    for (int i = 0; i < config->mSourcePortList.size(); i++) {
        uint32_t port = config->mSourcePortList[i];
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

int SetSportBlackFilter(std::shared_ptr<BPFWrapper<security_bpf>> wrapper,
                        int call_name_idx,
                        selector_filters& filters,
                        const SecurityNetworkFilter* config) {
    int idx = IdAllocator::GetInstance()->GetNextId<PortMap>();
    selector_filter k_filter;
    ::memset(&k_filter, 0, sizeof(k_filter));
    k_filter.filter_type = FILTER_TYPE_SPORT;
    k_filter.op_type = OP_TYPE_NOT_IN;
    k_filter.map_idx[0] = idx;
    if (filters.filter_count >= MAX_FILTER_FOR_PER_CALLNAME) {
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN, "filters count exceeded! max: %d\n", MAX_FILTER_FOR_PER_CALLNAME);
        return 1;
    }
    filters.filters[filters.filter_count++] = k_filter;

    if (config->mSourcePortBlackList.empty())
        return 0;
    for (int i = 0; i < config->mSourcePortBlackList.size(); i++) {
        uint32_t port = config->mSourcePortBlackList[i];
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

int SetDportFilter(std::shared_ptr<BPFWrapper<security_bpf>> wrapper,
                   int call_name_idx,
                   selector_filters& filters,
                   const SecurityNetworkFilter* config) {
    int idx = IdAllocator::GetInstance()->GetNextId<PortMap>();
    selector_filter k_filter;
    ::memset(&k_filter, 0, sizeof(k_filter));
    k_filter.filter_type = FILTER_TYPE_DPORT;
    k_filter.op_type = OP_TYPE_IN;
    k_filter.map_idx[0] = idx;
    if (filters.filter_count >= MAX_FILTER_FOR_PER_CALLNAME) {
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN, "filters count exceeded! max: %d\n", MAX_FILTER_FOR_PER_CALLNAME);
        return 1;
    }
    filters.filters[filters.filter_count++] = k_filter;

    if (config->mDestPortList.empty())
        return 0;
    for (int i = 0; i < config->mDestPortList.size(); i++) {
        uint32_t port = config->mDestPortList[i];
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

int SetDportBlackFilter(std::shared_ptr<BPFWrapper<security_bpf>> wrapper,
                        int call_name_idx,
                        selector_filters& filters,
                        const SecurityNetworkFilter* config) {
    int idx = IdAllocator::GetInstance()->GetNextId<PortMap>();
    selector_filter k_filter;
    ::memset(&k_filter, 0, sizeof(k_filter));
    k_filter.filter_type = FILTER_TYPE_DPORT;
    k_filter.op_type = OP_TYPE_NOT_IN;
    k_filter.map_idx[0] = idx;
    if (filters.filter_count >= MAX_FILTER_FOR_PER_CALLNAME) {
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_WARN, "filters count exceeded! max: %d\n", MAX_FILTER_FOR_PER_CALLNAME);
        return 1;
    }
    filters.filters[filters.filter_count++] = k_filter;

    if (config->mDestPortBlackList.empty())
        return 0;
    for (int i = 0; i < config->mDestPortBlackList.size(); i++) {
        uint32_t port = config->mDestPortBlackList[i];
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
    std::shared_ptr<BPFWrapper<security_bpf>> wrapper,
    const std::string& call_name,
    const std::variant<std::monostate, SecurityFileFilter, SecurityNetworkFilter> new_config) {
    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO,
             "EnableCallName %s idx: %d hold: %d\n",
             call_name.c_str(),
             new_config.index(),
             std::holds_alternative<SecurityFileFilter>(new_config));

    std::vector<AttachProgOps> attach_ops = {AttachProgOps("kprobe_" + call_name, true)};
    int ret = 0;
    int call_name_idx = GetCallNameIdx(call_name);
    if (call_name_idx < 0)
        return 1;
    auto filter = std::get_if<SecurityNetworkFilter>(&new_config);

    if (filter) {
        if (filter->mDestAddrBlackList.size() && filter->mDestAddrList.size())
            return 1;
        if (filter->mSourceAddrBlackList.size() && filter->mSourceAddrList.size())
            return 1;
        if (filter->mDestPortList.size() && filter->mDestPortBlackList.size())
            return 1;
        if (filter->mSourcePortList.size() && filter->mSourcePortBlackList.size())
            return 1;
        selector_filters kernel_filters;
        ::memset(&kernel_filters, 0, sizeof(kernel_filters));
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO, "filter not empty!\n");

        if (filter->mDestAddrList.size()) {
            SetDaddrFilter(wrapper, call_name_idx, kernel_filters, filter);
        }
        if (filter->mDestAddrBlackList.size()) {
            SetDaddrBlackFilter(wrapper, call_name_idx, kernel_filters, filter);
        }
        if (filter->mSourceAddrList.size()) {
            SetSaddrFilter(wrapper, call_name_idx, kernel_filters, filter);
        }
        if (filter->mSourceAddrBlackList.size()) {
            SetSaddrBlackFilter(wrapper, call_name_idx, kernel_filters, filter);
        }
        if (filter->mDestPortList.size()) {
            SetDportFilter(wrapper, call_name_idx, kernel_filters, filter);
        }
        if (filter->mDestPortBlackList.size()) {
            SetDportBlackFilter(wrapper, call_name_idx, kernel_filters, filter);
        }
        if (filter->mSourcePortList.size()) {
            SetSportFilter(wrapper, call_name_idx, kernel_filters, filter);
        }
        if (filter->mSourcePortBlackList.size()) {
            SetSportBlackFilter(wrapper, call_name_idx, kernel_filters, filter);
        }

        wrapper->UpdateBPFHashMap("filter_map", &call_name_idx, &kernel_filters, 0);
    }

    return ret;
}

int DeleteNetworkFilterForCallname(std::shared_ptr<BPFWrapper<security_bpf>> wrapper, const std::string& call_name) {
    std::vector<AttachProgOps> attach_ops = {AttachProgOps("kprobe_" + call_name, true)};
    ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO, "DisableCallName %s\n", call_name.c_str());

    int call_name_idx = GetCallNameIdx(call_name);
    if (call_name_idx < 0)
        return 1;
    int ret = 0;

    selector_filters kernel_filters;
    ::memset(&kernel_filters, 0, sizeof(kernel_filters));
    ret = wrapper->LookupBPFHashMap("filter_map", &call_name_idx, &kernel_filters);
    if (ret) {
        ebpf_log(eBPFLogType::NAMI_LOG_TYPE_INFO, "there is no filter for call name: %s\n", call_name.c_str());
        return 0;
    }

    for (int i = 0; i < kernel_filters.filter_count; i++) {
        auto filter = kernel_filters.filters[i];
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

    ::memset(&kernel_filters, 0, sizeof(kernel_filters));
    wrapper->UpdateBPFHashMap("filter_map", &call_name_idx, &kernel_filters, 0);

    return ret;
}

} // namespace ebpf
} // namespace logtail
