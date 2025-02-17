#pragma once

#include <coolbpf/security/bpf_process_event_type.h>
#include <coolbpf/security/type.h>

#include <string>
#include <vector>

namespace logtail {
namespace ebpf {

class SecurityEvent {
public:
    SecurityEvent(const std::string& call_name, const std::string& event_type, uint64_t ts)
        : call_name_(call_name), event_type_(event_type), timestamp_(ts) {}
    SecurityEvent(const std::string&& call_name, const std::string&& event_type, uint64_t ts)
        : call_name_(std::move(call_name)), event_type_(std::move(event_type)), timestamp_(ts) {}
    std::string call_name_;
    std::string event_type_;
    std::vector<std::pair<std::string, std::string>> tags_;
    uint64_t timestamp_;
};

class BaseSecurityNode {
public:
    BaseSecurityNode(uint32_t pid, uint64_t ktime) : mPid(pid), mKtime(ktime) {}
    void AddSecurityEvent(std::unique_ptr<SecurityEvent>&& event) { mEvents.emplace_back(std::move(event)); }
    virtual ~BaseSecurityNode() {}
    uint32_t mPid;
    uint64_t mKtime;
    mutable std::vector<std::unique_ptr<SecurityEvent>> mEvents;
};

class NetworkSecurityNode : public BaseSecurityNode {
public:
    NetworkSecurityNode(uint32_t _pid,
                        uint64_t ktime,
                        uint16_t& protocol,
                        uint16_t& family,
                        uint32_t& saddr,
                        uint32_t& daddr,
                        uint16_t& sport,
                        uint16_t& dport,
                        uint32_t& net_ns)
        : BaseSecurityNode(_pid, ktime),
          mProtocol(protocol),
          mFamily(family),
          mSaddr(saddr),
          mDaddr(daddr),
          mSport(sport),
          mDport(dport),
          mNetns(net_ns) {}
    // network attributes ...
    uint16_t mProtocol;
    uint16_t mFamily;
    uint32_t mSaddr; // Source address
    uint32_t mDaddr; // Destination address
    uint16_t mSport; // Source port
    uint16_t mDport; // Destination port
    uint32_t mNetns; // Network namespace
    // set in inner events ...
    // uint64_t timestamp;
    // uint16_t state;
    // uint64_t bytes;
};

class FileSecurityNode : public BaseSecurityNode {
public:
    FileSecurityNode(uint32_t _pid,
                     uint64_t ktime,
                     uint16_t& protocol,
                     uint16_t& family,
                     uint32_t& saddr,
                     uint32_t& daddr,
                     uint16_t& sport,
                     uint16_t& dport,
                     uint32_t& net_ns)
        : BaseSecurityNode(_pid, ktime),
          protocol_(protocol),
          family_(family),
          saddr_(saddr),
          daddr_(daddr),
          sport_(sport),
          dport_(dport),
          net_ns_(net_ns) {}
    // network attributes ...
    uint16_t protocol_;
    uint16_t family_;
    uint32_t saddr_; // Source address
    uint32_t daddr_; // Destination address
    uint16_t sport_; // Source port
    uint16_t dport_; // Destination port
    uint32_t net_ns_; // Network namespace
    // set in inner events ...
    // uint64_t timestamp;
    // uint16_t state;
    // uint64_t bytes;
};

class ProcessSecurityNode : public BaseSecurityNode {
public:
    ProcessSecurityNode(uint32_t _pid,
                        uint64_t ktime,
                        uint16_t& protocol,
                        uint16_t& family,
                        uint32_t& saddr,
                        uint32_t& daddr,
                        uint16_t& sport,
                        uint16_t& dport,
                        uint32_t& net_ns)
        : BaseSecurityNode(_pid, ktime),
          protocol_(protocol),
          family_(family),
          saddr_(saddr),
          daddr_(daddr),
          sport_(sport),
          dport_(dport),
          net_ns_(net_ns) {}
    // network attributes ...
    uint16_t protocol_;
    uint16_t family_;
    uint32_t saddr_; // Source address
    uint32_t daddr_; // Destination address
    uint16_t sport_; // Source port
    uint16_t dport_; // Destination port
    uint32_t net_ns_; // Network namespace
    // set in inner events ...
    // uint64_t timestamp;
    // uint16_t state;
    // uint64_t bytes;
};


// class BaseEventGroup {
// public:
//     virtual ~BaseEventGroup() {}
//     std::vector<std::unique_ptr<BaseSecurityNode>> events_;
// };

// class FileEventGroup : public BaseEventGroup {
// public:
//     FileEventGroup(const std::string& path) : path_(path) {}
//     FileEventGroup(const std::string&& path) : path_(std::move(path)) {}
//     void AddSecurityEvent(std::unique_ptr<BaseSecurityNode>&& event) { events_.emplace_back(std::move(event)); }
//     std::string path_;
// };

// class NetworkEventGroup : public BaseEventGroup {
// public:
//     NetworkEventGroup(uint16_t protocol,
//                       uint16_t family,
//                       uint32_t saddr,
//                       uint32_t daddr,
//                       uint16_t sport,
//                       uint16_t dport,
//                       uint32_t net_ns)
//         : protocol_(protocol),
//           family_(family),
//           saddr_(saddr),
//           daddr_(daddr),
//           sport_(sport),
//           dport_(dport),
//           net_ns_(net_ns) {}
//     void AddSecurityEvent(std::unique_ptr<BaseSecurityNode>&& event) { events_.emplace_back(std::move(event)); }
//     // network attributes ...
//     uint16_t protocol_;
//     uint16_t family_;
//     uint32_t saddr_; // Source address
//     uint32_t daddr_; // Destination address
//     uint16_t sport_; // Source port
//     uint16_t dport_; // Destination port
//     uint32_t net_ns_; // Network namespace
//     // set in inner events ...
//     // uint64_t timestamp;
//     // uint16_t state;
//     // uint64_t bytes;
// };


class BaseSecurityEvent {
public:
    BaseSecurityEvent(const struct msg_execve_key& _key, const struct msg_execve_key& _pkey) : key(_key), pkey(_pkey) {}
    ~BaseSecurityEvent() {}
    struct msg_execve_key key;
    struct msg_execve_key pkey;
};

class NetworkSecurityEvent : public BaseSecurityEvent {
public:
    NetworkSecurityEvent(tcp_data_t* event)
        : BaseSecurityEvent(event->key, event->pkey),
          func(event->func),
          protocol(event->protocol),
          state(event->state),
          family(event->family),
          pid(event->pid),
          saddr(event->saddr),
          daddr(event->daddr),
          sport(event->sport),
          dport(event->dport),
          net_ns(event->net_ns),
          timestamp(event->timestamp),
          bytes(event->bytes) {}
    enum sock_secure_func func;
    uint16_t protocol;
    uint16_t state;
    uint16_t family;
    uint32_t pid;
    uint32_t saddr; // Source address
    uint32_t daddr; // Destination address
    uint16_t sport; // Source port
    uint16_t dport; // Destination port
    uint32_t net_ns; // Network namespace
    uint64_t timestamp;
    uint64_t bytes;
};

// TODO
class FileSecurityEvent : public BaseSecurityEvent {};

class ProcessSecurityEvent : public BaseSecurityEvent {};


} // namespace ebpf
} // namespace logtail
