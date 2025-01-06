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
#include "ebpf/include/SysAkApi.h"
#include "ebpf/include/export.h"

namespace logtail {
namespace ebpf {

enum class eBPFPluginType {
    SOCKETTRACE = 0,
    PROCESS = 1,
    MAX = 2,
};

class SourceManager {
public:
    const std::string mDriverLibName = "ebpf_driver";

    SourceManager(const SourceManager&) = delete;
    SourceManager& operator=(const SourceManager&) = delete;

    void Init();

    bool StartPlugin(nami::PluginType plugin_type, std::unique_ptr<nami::eBPFConfig> conf);

    bool StopPlugin(nami::PluginType plugin_type);

    bool SuspendPlugin(nami::PluginType plugin_type);

    bool CheckPluginRunning(nami::PluginType plugin_type);

    SourceManager();
    ~SourceManager();

private:
    void FillCommonConf(std::unique_ptr<nami::eBPFConfig>& conf);
    bool LoadDynamicLib(const std::string& lib_name);
    bool DynamicLibSuccess();
    bool UpdatePlugin(nami::PluginType plugin_type, std::unique_ptr<nami::eBPFConfig> conf);

    enum class ebpf_func {
        EBPF_INIT,
        EBPF_UPDATE,
        EBPF_SUSPEND,
        EBPF_DEINIT,
        EBPF_REMOVE,
        EBPF_SOCKET_TRACE_CLEAN_UP_DOG,
        EBPF_SOCKET_TRACE_UPDATE_CONN_ADDR,
        EBPF_SOCKET_TRACE_DISABLE_PROCESS,
        EBPF_SOCKET_TRACE_UPDATE_CONN_ROLE,
        EBPF_MAX,
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
    std::array<void*, (int)ebpf_op_func::EBPF_OP_FUNC_MAX> mFuncs = {};
    std::array<long, (int)ebpf_func::EBPF_MAX> mOffsets = {};
    std::array<std::atomic_bool, (int)nami::PluginType::MAX> mRunning = {};
    std::string mHostIp;
    std::string mHostName;
    std::string mHostPathPrefix;
    std::string mBinaryPath;
    std::string mFullLibName;

#ifdef APSARA_UNIT_TEST_MAIN
    std::unique_ptr<nami::eBPFConfig> mConfig;
    friend class eBPFServerUnittest;
#endif
};

} // namespace ebpf
} // namespace logtail
