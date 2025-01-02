#pragma once

#include <string>
#include "ebpf/include/export.h"

namespace logtail {
namespace ebpf {


class SecurityEvent {
public:
  SecurityEvent(const std::string& call_name, const std::string& event_type, uint64_t ts) : call_name_(call_name), event_type_(event_type), timestamp_(ts) {}
  SecurityEvent(const std::string&& call_name, const std::string&& event_type, uint64_t ts) : call_name_(std::move(call_name)), event_type_(std::move(event_type)), timestamp_(ts) {}
  std::string call_name_;
  std::string event_type_;
  std::vector<std::pair<std::string, std::string>> tags_;
  uint64_t timestamp_;
};

class BaseEventGroup {
public:
  virtual ~BaseEventGroup() {}
  std::vector<std::unique_ptr<SecurityEvent>> events_;
};

class FileEventGroup : public BaseEventGroup {
public:
  FileEventGroup(const std::string& path) : path_(path) {}
  FileEventGroup(const std::string&& path) : path_(std::move(path)) {}
  void AddSecurityEvent(std::unique_ptr<SecurityEvent>&& event) {
    events_.emplace_back(std::move(event));
  }
  std::string path_;
};

class NetworkEventGroup : public BaseEventGroup {
public:
  NetworkEventGroup(uint16_t protocol, uint16_t family, uint32_t saddr, 
      uint32_t daddr, uint16_t sport, uint16_t dport, uint32_t net_ns) 
      : protocol_(protocol), family_(family), saddr_(saddr), daddr_(daddr), sport_(sport), dport_(dport), net_ns_(net_ns) {}
  void AddSecurityEvent(std::unique_ptr<SecurityEvent>&& event) {
    events_.emplace_back(std::move(event));
  }
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

class SecurityEventGroup {
public:
  SecurityEventGroup() {}
  // SecurityEventGroup(uint32_t& pid, uint64_t ktime) : pid_(pid), ktime_(ktime) {}
  SecurityEventGroup(const std::string& exec_id) : exec_id_(exec_id) {}
  std::vector<std::pair<std::string, std::string>> GetAllTags() { return tags_; }
  void AddSecurityEvent(std::unique_ptr<BaseEventGroup>&& event) {
    events_.emplace_back(std::move(event));
  }
  // for process ... 
  // uint32_t pid_;
  // uint64_t ktime_;
  std::string exec_id_;
  std::vector<std::pair<std::string, std::string>> tags_;
  std::vector<std::unique_ptr<BaseEventGroup>> events_;
};

class BaseSecurityNode {
public:
  BaseSecurityNode(uint32_t pid, uint64_t ktime) : pid_(pid), ktime_(ktime) {}
  void AddSecurityEvent(std::unique_ptr<SecurityEvent>&& event) {
    events_.emplace_back(std::move(event));
  }
  virtual ~BaseSecurityNode() {}
  uint32_t pid_;
  uint64_t ktime_;
  mutable std::vector<std::unique_ptr<SecurityEvent>> events_;
};

class NetworkSecurityNode : public BaseSecurityNode {
public:
  NetworkSecurityNode(uint32_t _pid, uint64_t ktime,
      uint16_t& protocol, uint16_t& family, uint32_t& saddr, 
      uint32_t& daddr, uint16_t& sport, uint16_t& dport, uint32_t& net_ns) 
      : BaseSecurityNode(_pid, ktime), protocol_(protocol), family_(family), saddr_(saddr), daddr_(daddr), sport_(sport), dport_(dport), net_ns_(net_ns) {}
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

}
}
