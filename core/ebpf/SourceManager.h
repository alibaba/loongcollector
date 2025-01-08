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

#include <chrono>
#include <cstring>
#include <dlfcn.h>

#include <array>
#include <atomic>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "common/DynamicLibHelper.h"
#include "ebpf/include/export.h"

#include "ebpf/driver/NetworkObserver.h"

namespace logtail {
namespace ebpf {

class SourceManager {
public:
    const std::string mDriverLibName = "ebpf_driver";

    SourceManager(const SourceManager&) = delete;
    SourceManager& operator=(const SourceManager&) = delete;

    void Init();

    bool StartPlugin(logtail::ebpf::PluginType plugin_type, std::unique_ptr<logtail::ebpf::PluginConfig> conf);

    bool StopPlugin(logtail::ebpf::PluginType plugin_type);

    bool SuspendPlugin(logtail::ebpf::PluginType plugin_type);

    bool CheckPluginRunning(logtail::ebpf::PluginType plugin_type);

    int32_t PollPerfBuffers(PluginType, int32_t, int32_t *, int);

    SourceManager();
    ~SourceManager();

    // int PrepareSkeleton(logtail::ebpf::PluginType type);
    // // Skeleton and program operations
    // int DestroySkeleton(logtail::ebpf::PluginType type);
    // int DynamicAttachBpfProg(logtail::ebpf::PluginType type, const std::string& prog_name);
    // int DynamicDetachBpfProg(logtail::ebpf::PluginType type, const std::string& prog_name);
    // int SetTailcall(logtail::ebpf::PluginType type, const std::string map_name, const std::vector<std::string>& funcs);

    // // Map operations
    // int SearchMapFd(logtail::ebpf::PluginType type, const std::string& map_name);
    // int CreateBpfMap(logtail::ebpf::PluginType type, enum bpf_map_type map_type, int key_size, int value_size, int max_entries, unsigned int map_flags);
    // int GetBpfMapFdById(logtail::ebpf::PluginType type, int id);

    // int LookupBpfMapElem(logtail::ebpf::PluginType type, const std::string& map_name, void* key, void* value);
    // int RemoveBpfMapElem(logtail::ebpf::PluginType type, const std::string& map_name, void* key);
    // int UpdateBpfMapElem(logtail::ebpf::PluginType type, const std::string& map_name, void* key, void* value, uint64_t flag);

    // // Perf buffer operations
    // int CreatePerfBuffer(logtail::ebpf::PluginType type, const std::string& map_name, int page_cnt, void* ctx, perf_process_event_fn data_cb, perf_loss_event_fn loss_cb);
    // int DeletePerfBuffer(logtail::ebpf::PluginType type, void* pb);
    // int PollPerfBuffer(logtail::ebpf::PluginType type, void* pb, int32_t max_events, int32_t* stop_flag, int time_out_ms);

    // /**
    //  * FOR NETWORK OBSERVER
    //  */
    // // Control plane
    // int InitNetworkObserver(const std::string& btf, const std::string& so_path, long uprobe_offset, long upca_offset, long upps_offset, long upcr_offset);
    // int StartNetworkObserver();
    // int StopNetworkObserver();
    // int NetworkObserverPollEvents(int32_t max_events, int32_t* stop_flag);
    // int NetworkObserverConfig(int32_t opt1, int32_t opt2, int32_t params_count, void** params, int32_t* params_len);

    // // Data plane
    // int NetworkObserverSetupNetDataProcessFunc(const std::function<void(void *, struct conn_data_event_t *)>& func, void* custom_data);
    // int NetworkObserverSetupNetEventProcessFunc(const std::function<void(void *, struct conn_ctrl_event_t *)>& func, void* custom_data);
    // int NetworkObserverSetupNetStatisticsProcessFunc(const std::function<void(void *, struct conn_stats_event_t *)>& func, void* custom_data);
    // int NetworkObserverSetupNetLostFunc(const std::function<void(void *, enum callback_type_e, uint64_t)>& func, void* custom_data);
    // int NetworkObserverSetupPrintFunc(const std::function<void(short level, const char *format, va_list args)>& func);

private:
    // void FillCommonConf(std::unique_ptr<logtail::ebpf::PluginConfig>& conf);
    bool LoadDynamicLib(const std::string& lib_name);
    bool LoadCoolBPF();
    bool DynamicLibSuccess();
    bool UpdatePlugin(logtail::ebpf::PluginType plugin_type, std::unique_ptr<logtail::ebpf::PluginConfig> conf);

    /*
    using set_logger_func = int(*)(logtail::ebpf::eBPFLogHandler fn);
    using start_plugin_func = int (*)(logtail::ebpf::PluginConfig *);
    using update_plugin_func = int (*)(logtail::ebpf::PluginConfig *);
    using stop_plugin_func = void (*)(logtail::ebpf::PluginType);
    using suspend_plugin_func = int (*)(logtail::ebpf::PluginType);
    using resume_plugin_func = int (*)(logtail::ebpf::PluginType);
    using poll_plugin_pbs_func = int(*)(logtail::ebpf::PluginType, int32_t, int32_t *, int);
    */
    enum class network_observer_uprobe_funcs {
        EBPF_NETWORK_OBSERVER_CLEAN_UP_DOG,
        EBPF_NETWORK_OBSERVER_UPDATE_CONN_ADDR,
        EBPF_NETWORK_OBSERVER_DISABLE_PROCESS,
        EBPF_NETWORK_OBSERVER_UPDATE_CONN_ROLE,
        EBPF_NETWORK_OBSERVER_MAX,
    };

    enum class ebpf_func {
        EBPF_SET_LOGGER,
        EBPF_START_PLUGIN,
        EBPF_UPDATE_PLUGIN,
        EBPF_STOP_PLUGIN,
        EBPF_SUSPEND_PLUGIN,
        EBPF_RESUME_PLUGIN,
        EBPF_POLL_PLUGIN_PBS,
        EBPF_FUNC_MAX,
    };

    enum class ebpf_op_func {
        PREPARE_SKELETON,
        DESTROY_SKELETON,
        DYNAMIC_ATTACH_BPF_PROG,
        DYNAMIC_DETACH_BPF_PROG,
        SET_TAILCALL,

        SEARCH_MAP_FD,
        GET_BPF_MAP_FD_BY_ID,
        CREATE_BPF_MAP,

        LOOKUP_BPF_MAP_ELEM,
        REMOVE_BPF_MAP_ELEM,
        UPDATE_BPF_MAP_ELEM,

        CREATE_PERF_BUFFER,
        DELETE_PERF_BUFFER,
        POLL_PERF_BUFFER,

        // Network Observer control plane
        INIT_NETWORK_OBSERVER,
        START_NETWORK_OBSERVER,
        STOP_NETWORK_OBSERVER,
        NETWORK_OBSERVER_POLL_EVENTS,
        NETWORK_OBSERVER_CONFIG,

        NETWORK_OBSERVER_CLEAN_UP_DOG,
        NETWORK_OBSERVER_UPDATE_CONN_ADDR,
        NETWORK_OBSERVER_DISABLE_PROCESS,
        NETWORK_OBSERVER_UPDATE_CONN_ROLE,

        // Network Observer data plane
        SETUP_NET_DATA_PROCESS_FUNC,
        SETUP_NET_EVENT_PROCESS_FUNC,
        SETUP_NET_STATISTICS_PROCESS_FUNC,
        SETUP_NET_LOST_FUNC,
        SETUP_PRINT_FUNC,

        EBPF_OP_FUNC_MAX,
    };

    std::shared_ptr<DynamicLibLoader> mLib;
    std::array<void*, (int)ebpf_func::EBPF_FUNC_MAX> mFuncs = {};
    std::array<long, (int)network_observer_uprobe_funcs::EBPF_NETWORK_OBSERVER_MAX> mOffsets = {};
    std::array<std::atomic_bool, (int)logtail::ebpf::PluginType::MAX> mRunning = {};
    std::string mHostIp;
    std::string mHostName;
    std::string mHostPathPrefix;
    std::string mBinaryPath;
    std::string mFullLibName;

    eBPFLogHandler mLogPrinter;

#ifdef APSARA_UNIT_TEST_MAIN
    std::unique_ptr<logtail::ebpf::PluginConfig> mConfig;
    friend class eBPFServerUnittest;
#endif
};

} // namespace ebpf
} // namespace logtail
