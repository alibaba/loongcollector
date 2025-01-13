#include "FileSecurityManager.h"

#include "ebpf/Config.h"
#include "ebpf/util/IdAllocator.h"
#include "logger/Logger.h"

namespace logtail {
namespace ebpf {

int FileSecurityManager::Init(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*> options) {
    // set init flag ...
    mFlag = true;

    int ret = 0;
    // step1. setup tail call
    // ret = wrapper_->SetTailCall("secure_tailcall_map", {"filter_prog", "secure_data_send"});
    LOG_DEBUG(sLogger, ("set tail call, ret", ret));


    // step1. setup perf buffer
    // 1. create perf buffer
    // std::vector<PerfBufferOps> perf_buffers = {
    //     PerfBufferOps("file_secure_output", 10*1024*1024, HandleFileKernelEvent, HandleFileKernelEventLoss),
    // };

    // 2. add pb to poller's task array ...
    // perf_workers_ = wrapper_->AttachPerfBuffers(this, perf_buffers, std::ref(flag_));
    LOG_DEBUG(sLogger, ("attach perf buffer, ret", ret));

    return ret;
}


int FileSecurityManager::EnableCallName(const std::string& callName, const configType newConfig) {
    int ret = 0;
    LOG_DEBUG(sLogger,
              ("EnableCallName", callName)("idx", newConfig.index())(
                  "hold", std::holds_alternative<logtail::ebpf::SecurityFileFilter>(newConfig)));

    int call_name_idx = GetCallNameIdx(callName);
    if (call_name_idx < 0)
        return 1;

    auto filter = std::get_if<logtail::ebpf::SecurityFileFilter>(&newConfig);
    // update filters map
    std::vector<path_entry> path_entries;
    // concatenate path and filename, then write the resulting char* path into path_filter_list
    // TODO qianlu.kk use map in map feature to support filters for different call names
    if (filter && filter->mFilePathList.size()) {
        selector_filters kernel_filters;
        ::memset(&kernel_filters, 0, sizeof(kernel_filters));

        int idx = IdAllocator::GetInstance()->GetNextId<logtail::ebpf::StringPrefixMap>();
        if (idx < 0) {
            LOG_WARNING(sLogger,
                        ("Failed to get next id, reach max",
                         IdAllocator::GetInstance()->GetMaxId<logtail::ebpf::StringPrefixMap>()));
            return 1;
        }
        LOG_DEBUG(sLogger, ("call_name", callName)("index", idx));
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

        LOG_DEBUG(sLogger, ("filter not empty!", ""));
        for (size_t i = 0; i < filter->mFilePathList.size() && i < MAX_FILTER_FOR_PER_CALLNAME; i++) {
            auto& x = filter->mFilePathList[i];
            LOG_DEBUG(sLogger, ("path", x)("begin to update map in map for filter detail, idx", idx));

            // update inner map
            string_prefix_lpm_trie prefix_trie;
            ::memset(&prefix_trie, 0, sizeof(prefix_trie));
            ::memcpy(prefix_trie.data, x.data(), x.length());
            prefix_trie.prefixlen = x.length() * 8; // in bits
            // uint8_t val(1);
            LOG_DEBUG(sLogger,
                      ("[before update] prefix trie data", prefix_trie.data)("prefix_len", prefix_trie.prefixlen));
            // TODO @qianlu.kk update inner map
            // ret = wrapper_->UpdateInnerMapElem<logtail::ebpf::StringPrefixMap>(std::string("string_prefix_maps"),
            // &idx, &prefix_trie, &val, 0);
            if (ret) {
                LOG_DEBUG(sLogger,
                          ("[after update] prefix trie data failed! data", prefix_trie.data)("prefix_len",
                                                                                             prefix_trie.prefixlen));
                continue;
            }
        }

        // TODO @qianlu.kk update inner map
        // udpate filter_map
        // wrapper_->UpdateBPFHashMap("filter_map", &call_name_idx, &kernel_filters, 0);
    }

    // TODO @qianlu.kk
    // std::vector<AttachProgOps> attach_ops = {AttachProgOps("kprobe_" + call_name, true)};
    // ret = wrapper_->DynamicAttachBPFObject(attach_ops);

    return ret;
}


int FileSecurityManager::DisableCallName(const std::string& callName) {
    LOG_DEBUG(sLogger, ("DisableCallName", callName));

    int call_name_idx = GetCallNameIdx(callName);
    if (call_name_idx < 0)
        return 1;
    int ret = 0;
    // step1: detach callname
    // std::vector<AttachProgOps> attach_ops = {AttachProgOps("kprobe_" + call_name, true)};
    // ret = wrapper_->DynamicDetachBPFObject(attach_ops);

    // step2: get filters for call name
    selector_filters kernel_filters;
    ::memset(&kernel_filters, 0, sizeof(kernel_filters));
    // get filters
    // ret = wrapper_->LookupBPFHashMap("filter_map", &call_name_idx, &kernel_filters);
    if (ret) {
        // no filters found, return directly
        LOG_WARNING(sLogger, ("There is no filter for call name", callName));
        return 0;
    }

    // step3: remove filters
    for (int i = 0; i < kernel_filters.filter_count; i++) {
        auto filter = kernel_filters.filters[i];
        assert(filter.filter_type == FILTER_TYPE_FILE_PREFIX);
        auto outter_key = filter.map_idx[0];
        // wrapper_->DeleteInnerMap<logtail::ebpf::StringPrefixMap>("string_prefix_maps", &outter_key);
        IdAllocator::GetInstance()->ReleaseId<logtail::ebpf::StringPrefixMap>(outter_key);
        LOG_DEBUG(sLogger, ("Release filter for type", (int)filter.filter_type)("map_idx", outter_key));
    }

    // step4: delete filter map for call name
    ::memset(&kernel_filters, 0, sizeof(kernel_filters));
    // ret = wrapper_->UpdateBPFHashMap("filter_map", &call_name_idx, &kernel_filters, 0);

    return ret;
}

} // namespace ebpf
} // namespace logtail
