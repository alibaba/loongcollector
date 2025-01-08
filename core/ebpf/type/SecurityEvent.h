#pragma once

#include <string>

#include "ebpf/include/export.h"
#include <coolbpf/security/bpf_process_event_type.h>

namespace logtail {
namespace ebpf {

/**
 * eBPF Type for Kernel Event
 */
struct MsgCommon {
  uint8_t op;
  uint8_t flags;
  uint8_t pad_v2[2];
  uint32_t size;
  uint64_t ktime;
};

struct MsgExecveKey {
  uint32_t pid;
  uint32_t pad;
  uint64_t ktime;
};

struct MsgCapabilities {
  uint64_t permitted;
  uint64_t effective;
  uint64_t inheritable;
};

struct MsgUserNamespace {
  int32_t level;
  uint32_t uid;
  uint32_t gid;
  uint32_t ns_inum;
};

struct MsgGenericCred {
  uint32_t uid;
  uint32_t gid;
  uint32_t suid;
  uint32_t sgid;
  uint32_t euid;
  uint32_t egid;
  uint32_t fsuid;
  uint32_t fsgid;
  uint32_t secure_bits;
  uint32_t pad;
  MsgCapabilities cap;
  MsgUserNamespace user_ns;
};

struct MsgK8s {
  uint32_t net_ns;
  uint32_t cid;
  uint64_t cgrpid;
  char docker[DOCKER_ID_LENGTH];
//  std::array<char, DOCKER_ID_LENGTH> docker;
};

struct MsgNamespaces {
  uint32_t uts_inum;
  uint32_t ipc_inum;
  uint32_t mnt_inum;
  uint32_t pid_inum;
  uint32_t pid_child_inum;
  uint32_t net_inum;
  uint32_t time_inum;
  uint32_t time_child_inum;
  uint32_t cgroup_inum;
  uint32_t user_inum;
};

struct MsgExecveEvent {
public:
  MsgCommon common;
  MsgK8s kube;
  MsgExecveKey parent;
  uint64_t parent_flags;
  MsgGenericCred creds;
  MsgNamespaces namespaces;
  MsgExecveKey cleanup_process;
};

struct MsgK8sUnix {
  std::string docker;
};

struct MsgUserRecord {
  std::string name;
};

struct MsgProcess {
  uint32_t size;
  uint32_t pid;
  uint32_t tid;
  uint32_t nspid;
  uint32_t secure_exec;
  uint32_t uid;
  uint32_t auid;
  uint32_t flags;
  uint32_t nlink;
  uint64_t ino;
  uint64_t ktime;
  std::string filename;
  std::string args;
  std::string cmdline;
  std::string cwd;
  MsgUserRecord user;
};

class MsgExecveEventUnix {
public:
  MsgExecveEventUnix() : msg(nullptr) {}
  std::unique_ptr<MsgExecveEvent> msg;
  MsgK8sUnix kube;
  MsgProcess process;
  std::string exec_id;
  std::string parent_exec_id;
  bool kernel_thread;
  std::string tags;
  inline void print() const;
};

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

class FileSecurityNode : public BaseSecurityNode {
public:
  FileSecurityNode(uint32_t _pid, uint64_t ktime,
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

class ProcessSecurityNode : public BaseSecurityNode {
public:
  ProcessSecurityNode(uint32_t _pid, uint64_t ktime,
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
