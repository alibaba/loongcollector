#pragma once

#include <cstdint>
#include <stddef.h>
#include <string>
#include <vector>
#include <memory>

#include "ebpf/driver/coolbpf/src/security/bpf_process_event_type.h"
#include "CommonDataEvent.h"

namespace logtail {
namespace ebpf {

class NetworkEvent : public CommonEvent {
public:
    NetworkEvent(uint32_t pid, uint64_t ktime, KernelEventType type, uint64_t timestamp, uint16_t protocol,
                        uint16_t family,
                        uint32_t saddr,
                        uint32_t daddr,
                        uint16_t sport,
                        uint16_t dport,
                        uint32_t net_ns) : CommonEvent(pid,ktime,type,timestamp), mProtocol(protocol),
          mFamily(family),
          mSport(sport),
          mDport(dport),
          mSaddr(saddr),
          mDaddr(daddr),
          mNetns(net_ns) {}
    virtual PluginType GetPluginType() const { return PluginType::NETWORK_SECURITY; };
    uint16_t mProtocol;
    uint16_t mFamily;
    uint16_t mSport; // Source port
    uint16_t mDport; // Destination port
    uint32_t mSaddr; // Source address
    uint32_t mDaddr; // Destination address
    uint32_t mNetns; // Network namespace
};

class NetworkEventGroup {
public:
    NetworkEventGroup(uint32_t pid, uint64_t ktime, uint16_t protocol,
                        uint16_t family,
                        uint32_t saddr,
                        uint32_t daddr,
                        uint16_t sport,
                        uint16_t dport,
                        uint32_t net_ns) : mPid(pid), mKtime(ktime), mProtocol(protocol),
          mFamily(family),
          mSport(sport),
          mDport(dport),
          mSaddr(saddr),
          mDaddr(daddr),
          mNetns(net_ns) {}
    uint32_t mPid;
    uint64_t mKtime;
    uint16_t mProtocol;
    uint16_t mFamily;
    uint16_t mSport; // Source port
    uint16_t mDport; // Destination port
    uint32_t mSaddr; // Source address
    uint32_t mDaddr; // Destination address
    uint32_t mNetns; // Network namespace
    // attrs
    std::vector<std::shared_ptr<NetworkEvent>> mInnerEvents;
};

}
}
