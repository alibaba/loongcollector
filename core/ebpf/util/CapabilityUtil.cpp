//
// Created by qianlu on 2024/6/26.
//

#include "CapabilityUtil.h"
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <sstream>

namespace logtail {
namespace ebpf {

std::unordered_map<uint64_t, std::string> capabilitiesString = {
  {0, "CAP_CHOWN"},
  {1, "DAC_OVERRIDE"},
  {2, "CAP_DAC_READ_SEARCH"},
  {3, "CAP_FOWNER"},
  {4, "CAP_FSETID"},
  {5, "CAP_KILL"},
  {6, "CAP_SETGID"},
  {7, "CAP_SETUID"},
  {8, "CAP_SETPCAP"},
  {9, "CAP_LINUX_IMMUTABLE"},
  {10, "CAP_NET_BIND_SERVICE"},
  {11, "CAP_NET_BROADCAST"},
  {12, "CAP_NET_ADMIN"},
  {13, "CAP_NET_RAW"},
  {14, "CAP_IPC_LOCK"},
  {15, "CAP_IPC_OWNER"},
  {16, "CAP_SYS_MODULE"},
  {17, "CAP_SYS_RAWIO"},
  {18, "CAP_SYS_CHROOT"},
  {19, "CAP_SYS_PTRACE"},
  {20, "CAP_SYS_PACCT"},
  {21, "CAP_SYS_ADMIN"},
  {22, "CAP_SYS_BOOT"},
  {23, "CAP_SYS_NICE"},
  {24, "CAP_SYS_RESOURCE"},
  {25, "CAP_SYS_TIME"},
  {26, "CAP_SYS_TTY_CONFIG"},
  {27, "CAP_MKNOD"},
  {28, "CAP_LEASE"},
  {29, "CAP_AUDIT_WRITE"},
  {30, "CAP_AUDIT_CONTROL"},
  {31, "CAP_SETFCAP"},
  {32, "CAP_MAC_OVERRIDE"},
  {33, "CAP_MAC_ADMIN"},
  {34, "CAP_SYSLOG"},
  {35, "CAP_WAKE_ALARM"},
  {36, "CAP_BLOCK_SUSPEND"},
  {37, "CAP_AUDIT_READ"},
  {38, "CAP_PERFMON"},
  {39, "CAP_BPF"},
  {40, "CAP_CHECKPOINT_RESTORE"}
};

const int32_t CAP_LAST_CAP = 40;

bool IsCapValid(int32_t capInt) {
  return (capInt >= 0 && capInt <= CAP_LAST_CAP);
}

std::string GetCapability(int32_t capInt) {
  if (!IsCapValid(capInt)) {
    throw std::invalid_argument("invalid capability value " + std::to_string(capInt));
  }

  auto it = capabilitiesString.find(static_cast<uint64_t>(capInt));
  if (it == capabilitiesString.end()) {
    throw std::invalid_argument("could not map capability value " + std::to_string(capInt));
  }

  return it->second;
}

std::string GetCapabilities(uint64_t capInt) {
  std::vector<std::string> caps;

  for (uint64_t i = 0; i < 64; ++i) {
    if ((1ULL << i) & capInt) {
      auto it = capabilitiesString.find(i);
      if (it != capabilitiesString.end()) {
        caps.push_back(it->second);
      }
    }
  }

  std::ostringstream oss;
  for (size_t i = 0; i < caps.size(); ++i) {
    if (i > 0) {
      oss << " ";
    }
    oss << caps[i];
  }
  return oss.str();
}

}
}
