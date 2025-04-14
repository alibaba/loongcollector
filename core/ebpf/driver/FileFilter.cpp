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

extern "C" {
#include <bpf/libbpf.h>
#include <coolbpf/coolbpf.h>
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include <coolbpf/security.skel.h>
#pragma GCC diagnostic pop
#include <unistd.h>

#include <string>
#include <vector>

#include "BPFMapTraits.h"
#include "CallName.h"
#include "FileFilter.h"
#include "IdAllocator.h"
#include "Log.h"
#include "eBPFWrapper.h"
#include "ebpf/include/export.h"

namespace logtail {
namespace ebpf {

int CreateFileFilterForCallname(std::shared_ptr<logtail::ebpf::BPFWrapper<security_bpf>> wrapper,
                                const std::string& call_name,
                                const std::variant<std::monostate, SecurityFileFilter, SecurityNetworkFilter> config) {
    ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_INFO,
             "[CreateFilterForCallname] EnableCallName:%s, idx:%ld, hold:%d \n",
             call_name.c_str(),
             config.index(),
             std::holds_alternative<SecurityFileFilter>(config));
    int ret = 0;

    int call_name_idx = GetCallNameIdx(call_name);
    if (call_name_idx < 0)
        return 1;

    auto filter = std::get_if<SecurityFileFilter>(&config);
    // update filters map
    std::vector<path_entry> path_entries;
    // concatenate path and filename, then write the resulting char* path into path_filter_list
    // TODO qianlu.kk use map in map feature to support filters for different call names
    if (filter && filter->mFilePathList.size()) {
        selector_filters kernel_filters;
        ::memset(&kernel_filters, 0, sizeof(kernel_filters));

        int idx = IdAllocator::GetInstance()->GetNextId<StringPrefixMap>();
        if (idx < 0) {
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                     "[CreateFilterForCallname][IDAllocator] Failed to get next id, reach max %d\n",
                     IdAllocator::GetInstance()->GetMaxId<StringPrefixMap>());
            return 1;
        }
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                 "[CreateFilterForCallname] Get index %d for %s\n",
                 idx,
                 call_name.c_str());
        // step1: add a new entry into string_prefix_maps, and assign a filter id
        // step2: add a filter into filter map and record filter type and filter id
        selector_filter k_filter;
        ::memset(&k_filter, 0, sizeof(k_filter));
        k_filter.filter_type = FILTER_TYPE_FILE_PREFIX;
        k_filter.map_idx[0] = idx;
        // in bytes
        // k_filter.vallen = x.length();
        kernel_filters.filter_count = 1;
        kernel_filters.filters[0] = k_filter;

        // LOG(INFO) << "filter not empty!";
        for (int i = 0; i < (int)filter->mFilePathList.size() && i < MAX_FILTER_FOR_PER_CALLNAME; i++) {
            const auto& x = filter->mFilePathList[i];

            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                     "[CreateFilterForCallname] begin to update map in map for filter detail, idx: %d, path: %s\n",
                     idx,
                     x.c_str());

            // update inner map
            string_prefix_lpm_trie prefix_trie;
            ::memset(&prefix_trie, 0, sizeof(prefix_trie));
            ::memcpy(prefix_trie.data, x.data(), x.length());
            prefix_trie.prefixlen = x.length() * 8; // in bits
            uint8_t val = 1;
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                     "[CreateFilterForCallname][before update] prefix trie data: %s prefix_len: %u\n",
                     prefix_trie.data,
                     prefix_trie.prefixlen);
            ret = wrapper->UpdateInnerMapElem<StringPrefixMap>(
                std::string("string_prefix_maps"), &idx, &prefix_trie, &val, 0);
            if (ret) {
                ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                         "[CreateFilterForCallname][update failed] prefix trie data: %s prefix_len: %u\n",
                         prefix_trie.data,
                         prefix_trie.prefixlen);
                continue;
            }
        }

        // udpate filter_map
        wrapper->UpdateBPFHashMap("filter_map", &call_name_idx, &kernel_filters, 0);
    }

    return ret;
}

int DeleteFileFilterForCallname(std::shared_ptr<logtail::ebpf::BPFWrapper<security_bpf>> wrapper,
                                const std::string& call_name) {
    ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN, "DeleteFilterForCallname %s\n", call_name.c_str());
    int call_name_idx = GetCallNameIdx(call_name);
    if (call_name_idx < 0)
        return 1;
    int ret = 0;
    // step1: detach callname

    // step2: get filters for call name
    selector_filters kernel_filters;
    ::memset(&kernel_filters, 0, sizeof(kernel_filters));
    // get filters
    ret = wrapper->LookupBPFHashMap("filter_map", &call_name_idx, &kernel_filters);
    if (ret) {
        // no filters found, return directly
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_INFO,
                 "[DeleteFilterForCallname] there is no filter for call name: %s\n",
                 call_name.c_str());
        return 0;
    }

    // step3: remove filters
    for (int i = 0; i < kernel_filters.filter_count; i++) {
        auto filter = kernel_filters.filters[i];
        auto outter_key = filter.map_idx[0];
        wrapper->DeleteInnerMap<StringPrefixMap>("string_prefix_maps", &outter_key);
        IdAllocator::GetInstance()->ReleaseId<StringPrefixMap>(outter_key);
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                 "[DeleteFilterForCallname] release filter for type: %d mapIdx: %u\n",
                 static_cast<int>(filter.filter_type),
                 outter_key);
    }

    // step4: delete filter map for call name
    ::memset(&kernel_filters, 0, sizeof(kernel_filters));
    ret = wrapper->UpdateBPFHashMap("filter_map", &call_name_idx, &kernel_filters, 0);

    return ret;
}

} // namespace ebpf
} // namespace logtail
