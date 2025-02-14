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

#pragma once

extern "C" {
#include <bpf/libbpf.h>
#include <coolbpf/coolbpf.h>
};

#include <coolbpf/security.skel.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "BPFMapTraits.h"
#include "IdAllocator.h"
#include "Log.h"
#include "eBPFWrapper.h"
#include "ebpf/include/export.h"

namespace logtail {
namespace ebpf {

int CreateFileFilterForCallname(std::shared_ptr<logtail::ebpf::BPFWrapper<security_bpf>> wrapper,
                                const std::string& call_name,
                                const std::variant<std::monostate, SecurityFileFilter, SecurityNetworkFilter> config);

int DeleteFileFilterForCallname(std::shared_ptr<logtail::ebpf::BPFWrapper<security_bpf>> wrapper,
                                const std::string& call_name);

} // namespace ebpf
} // namespace logtail
