// Copyright 2025 iLogtail Authors
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
// #include <bpf/libbpf.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include <coolbpf/security.skel.h>
#pragma GCC diagnostic pop
};

#include <unistd.h>

#include <string>

#include "ebpf/driver/BPFWrapper.h"
#include "ebpf/include/export.h"

namespace logtail {
namespace ebpf {

#ifdef APSARA_UNIT_TEST_MAIN
struct TestPortFilterItem {
    int mIdx;
    uint32_t mPort;
    enum op_type mOpType;
    uint8_t mVal;
};

struct TestAddr4FilterItem {
    int mIdx;
    addr4_lpm_trie mArg4;
    enum op_type mOpType;
    uint8_t mVal;
};

struct TestAddr6FilterItem {
    int mIdx;
    addr6_lpm_trie mArg6;
    enum op_type mOpType;
    uint8_t mVal;
};

std::vector<TestPortFilterItem> GetPortFilters();
std::vector<TestAddr4FilterItem> GetAddr4Filters();
std::vector<TestAddr6FilterItem> GetAddr6Filters();

#endif

int CreateNetworkFilterForCallname(
    std::shared_ptr<logtail::ebpf::BPFWrapper<security_bpf>>& wrapper,
    const std::string& callName,
    const std::variant<std::monostate, SecurityFileFilter, SecurityNetworkFilter>& newConfig);

int DeleteNetworkFilterForCallname(std::shared_ptr<logtail::ebpf::BPFWrapper<security_bpf>>& wrapper,
                                   const std::string& callName);


int SetSaddrFilter(std::shared_ptr<BPFWrapper<security_bpf>>& wrapper,
                   int /*callNameIdx*/,
                   selector_filters& filters,
                   const SecurityNetworkFilter* config);

int SetSaddrBlackFilter(std::shared_ptr<BPFWrapper<security_bpf>>& wrapper,
                        int /*callNameIdx*/,
                        selector_filters& filters,
                        const SecurityNetworkFilter* config);

int SetDaddrFilter(std::shared_ptr<BPFWrapper<security_bpf>>& wrapper,
                   int /*callNameIdx*/,
                   selector_filters& filters,
                   const SecurityNetworkFilter* config);

int SetDaddrBlackFilter(std::shared_ptr<BPFWrapper<security_bpf>>& wrapper,
                        int /*callNameIdx*/,
                        selector_filters& filters,
                        const SecurityNetworkFilter* config);

int SetSportFilter(std::shared_ptr<BPFWrapper<security_bpf>>& wrapper,
                   int /*callNameIdx*/,
                   selector_filters& filters,
                   const SecurityNetworkFilter* config);

int SetSportBlackFilter(std::shared_ptr<BPFWrapper<security_bpf>>& wrapper,
                        int /*callNameIdx*/,
                        selector_filters& filters,
                        const SecurityNetworkFilter* config);

int SetDportFilter(std::shared_ptr<BPFWrapper<security_bpf>>& wrapper,
                   int /*callNameIdx*/,
                   selector_filters& filters,
                   const SecurityNetworkFilter* config);

int SetDportBlackFilter(std::shared_ptr<BPFWrapper<security_bpf>>& wrapper,
                        int /*callNameIdx*/,
                        selector_filters& filters,
                        const SecurityNetworkFilter* config);


} // namespace ebpf
} // namespace logtail
