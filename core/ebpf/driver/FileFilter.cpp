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
                                const std::string& callName,
                                const std::variant<std::monostate, SecurityFileFilter, SecurityNetworkFilter> config) {
    ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_INFO,
             "[CreateFilterForCallname] EnableCallName:%s, idx:%ld, hold:%d \n",
             callName.c_str(),
             config.index(),
             std::holds_alternative<SecurityFileFilter>(config));
    int ret = 0;

    int callNameIdx = GetCallNameIdx(callName);
    if (callNameIdx == ERR_UNKNOWN_CALLNAME) {
        return ERR_DRIVER_INVALID_PARAM;
    }

    auto filter = std::get_if<SecurityFileFilter>(&config);
    // update filters map
    std::vector<path_entry> path_entries;
    // concatenate path and filename, then write the resulting char* path into path_filter_list
    // TODO qianlu.kk use map in map feature to support filters for different call names
    if (filter && filter->mFilePathList.size()) {
        selector_filters kernelFilters;
        ::memset(&kernelFilters, 0, sizeof(kernelFilters));

        int idx = IdAllocator::GetInstance()->GetNextId<StringPrefixMap>();
        if (idx == ERR_LIMIT_EXCEEDED) {
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                     "[CreateFilterForCallname][IDAllocator] Failed to get next id, reach max %d\n",
                     IdAllocator::GetInstance()->GetMaxId<StringPrefixMap>());
            return ERR_DRIVER_INVALID_PARAM;
        }
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                 "[CreateFilterForCallname] Get index %d for %s\n",
                 idx,
                 callName.c_str());
        // step1: add a new entry into string_prefix_maps, and assign a filter id
        // step2: add a filter into filter map and record filter type and filter id
        selector_filter kFilter;
        ::memset(&kFilter, 0, sizeof(kFilter));
        kFilter.filter_type = FILTER_TYPE_FILE_PREFIX;
        kFilter.map_idx[0] = idx;
        // in bytes
        // kFilter.vallen = x.length();
        kernelFilters.filter_count = 1;
        kernelFilters.filters[0] = kFilter;

        // LOG(INFO) << "filter not empty!";
        for (int i = 0; i < (int)filter->mFilePathList.size() && i < MAX_FILTER_FOR_PER_CALLNAME; i++) {
            const auto& x = filter->mFilePathList[i];

            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                     "[CreateFilterForCallname] begin to update map in map for filter detail, idx: %d, path: %s\n",
                     idx,
                     x.c_str());

            // update inner map
            string_prefix_lpm_trie prefixTrie;
            ::memset(&prefixTrie, 0, sizeof(prefixTrie));
            ::memcpy(prefixTrie.data, x.data(), x.length());
            prefixTrie.prefixlen = x.length() * 8; // in bits
            uint8_t val = 1;
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                     "[CreateFilterForCallname][before update] prefix trie data: %s prefix_len: %u\n",
                     prefixTrie.data,
                     prefixTrie.prefixlen);
            ret = wrapper->UpdateInnerMapElem<StringPrefixMap>(
                std::string("string_prefix_maps"), &idx, &prefixTrie, &val, 0);
            if (ret) {
                ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                         "[CreateFilterForCallname][update failed] prefix trie data: %s prefix_len: %u\n",
                         prefixTrie.data,
                         prefixTrie.prefixlen);
                continue;
            }
        }

        // udpate filter_map
        wrapper->UpdateBPFHashMap("filter_map", &callNameIdx, &kernelFilters, 0);
    }

    return ret;
}

int DeleteFileFilterForCallname(std::shared_ptr<logtail::ebpf::BPFWrapper<security_bpf>> wrapper,
                                const std::string& callName) {
    ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN, "DeleteFilterForCallname %s\n", callName.c_str());
    int callNameIdx = GetCallNameIdx(callName);
    if (callNameIdx == ERR_UNKNOWN_CALLNAME) {
        return ERR_DRIVER_INVALID_PARAM;
    }
    int ret = 0;
    // step1: detach callname

    // step2: get filters for call name
    selector_filters kernelFilters;
    ::memset(&kernelFilters, 0, sizeof(kernelFilters));
    // get filters
    ret = wrapper->LookupBPFHashMap("filter_map", &callNameIdx, &kernelFilters);
    if (ret) {
        // no filters found, return directly
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_INFO,
                 "[DeleteFilterForCallname] there is no filter for call name: %s\n",
                 callName.c_str());
        return 0;
    }

    // step3: remove filters
    for (int i = 0; i < kernelFilters.filter_count; i++) {
        auto filter = kernelFilters.filters[i];
        auto outterKey = filter.map_idx[0];
        wrapper->DeleteInnerMap<StringPrefixMap>("string_prefix_maps", &outterKey);
        IdAllocator::GetInstance()->ReleaseId<StringPrefixMap>(outterKey);
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                 "[DeleteFilterForCallname] release filter for type: %d mapIdx: %u\n",
                 static_cast<int>(filter.filter_type),
                 outterKey);
    }

    // step4: delete filter map for call name
    ::memset(&kernelFilters, 0, sizeof(kernelFilters));
    ret = wrapper->UpdateBPFHashMap("filter_map", &callNameIdx, &kernelFilters, 0);

    return ret;
}

} // namespace ebpf
} // namespace logtail
