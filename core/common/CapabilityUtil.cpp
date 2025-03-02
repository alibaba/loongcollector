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

#include "CapabilityUtil.h"

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>

#include "common/memory/SourceBuffer.h"

namespace logtail {

static constexpr std::array kCapabilityStrings = {std::string_view("CAP_CHOWN"),
                                                  std::string_view("DAC_OVERRIDE"),
                                                  std::string_view("CAP_DAC_READ_SEARCH"),
                                                  std::string_view("CAP_FOWNER"),
                                                  std::string_view("CAP_FSETID"),
                                                  std::string_view("CAP_KILL"),
                                                  std::string_view("CAP_SETGID"),
                                                  std::string_view("CAP_SETUID"),
                                                  std::string_view("CAP_SETPCAP"),
                                                  std::string_view("CAP_LINUX_IMMUTABLE"),
                                                  std::string_view("CAP_NET_BIND_SERVICE"),
                                                  std::string_view("CAP_NET_BROADCAST"),
                                                  std::string_view("CAP_NET_ADMIN"),
                                                  std::string_view("CAP_NET_RAW"),
                                                  std::string_view("CAP_IPC_LOCK"),
                                                  std::string_view("CAP_IPC_OWNER"),
                                                  std::string_view("CAP_SYS_MODULE"),
                                                  std::string_view("CAP_SYS_RAWIO"),
                                                  std::string_view("CAP_SYS_CHROOT"),
                                                  std::string_view("CAP_SYS_PTRACE"),
                                                  std::string_view("CAP_SYS_PACCT"),
                                                  std::string_view("CAP_SYS_ADMIN"),
                                                  std::string_view("CAP_SYS_BOOT"),
                                                  std::string_view("CAP_SYS_NICE"),
                                                  std::string_view("CAP_SYS_RESOURCE"),
                                                  std::string_view("CAP_SYS_TIME"),
                                                  std::string_view("CAP_SYS_TTY_CONFIG"),
                                                  std::string_view("CAP_MKNOD"),
                                                  std::string_view("CAP_LEASE"),
                                                  std::string_view("CAP_AUDIT_WRITE"),
                                                  std::string_view("CAP_AUDIT_CONTROL"),
                                                  std::string_view("CAP_SETFCAP"),
                                                  std::string_view("CAP_MAC_OVERRIDE"),
                                                  std::string_view("CAP_MAC_ADMIN"),
                                                  std::string_view("CAP_SYSLOG"),
                                                  std::string_view("CAP_WAKE_ALARM"),
                                                  std::string_view("CAP_BLOCK_SUSPEND"),
                                                  std::string_view("CAP_AUDIT_READ"),
                                                  std::string_view("CAP_PERFMON"),
                                                  std::string_view("CAP_BPF"),
                                                  std::string_view("CAP_CHECKPOINT_RESTORE")};

StringView GetCapabilities(uint64_t capInt, SourceBuffer& sb) {
    if (capInt == 0) {
        return StringView("");
    }

    size_t capLen = 0;
    for (uint64_t i = 0; i < kCapabilityStrings.size(); ++i) {
        if ((1ULL << i) & capInt) {
            if (capLen != 0) {
                ++capLen;
            }
            capLen += kCapabilityStrings[i].size();
        }
    }

    auto result = sb.AllocateStringBuffer(capLen);
    for (uint64_t i = 0; i < kCapabilityStrings.size(); ++i) {
        if ((1ULL << i) & capInt) {
            if (result.size != 0) {
                memcpy(result.data + result.size, " ", 1);
                ++result.size;
            }
            memcpy(result.data + result.size, kCapabilityStrings[i].data(), kCapabilityStrings[i].size());
            result.size += kCapabilityStrings[i].size();
        }
    }

    return {result.data, result.size};
}

} // namespace logtail
