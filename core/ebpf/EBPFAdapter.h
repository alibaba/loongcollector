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

#include <cstring>
#include <dlfcn.h>

#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "common/DynamicLibHelper.h"
#include "ebpf/include/export.h"

namespace logtail::ebpf {

inline constexpr int kDefaultMaxBatchConsumeSize = 1024;
inline constexpr int kDefaultMaxWaitTimeMS = 200;

class EBPFAdapter {
public:
    const std::string mDriverLibName = "eBPFDriver";

    EBPFAdapter(const EBPFAdapter&) = delete;
    EBPFAdapter& operator=(const EBPFAdapter&) = delete;

    void Init();

    bool StartPlugin(PluginType pluginType, std::unique_ptr<PluginConfig> conf);

    bool StopPlugin(PluginType pluginType);

    // detach bpf progs ...
    bool SuspendPlugin(PluginType pluginType);

    // just update configs ...
    bool UpdatePlugin(PluginType pluginType, std::unique_ptr<PluginConfig> conf);

    // re-attach bpf progs ...
    bool ResumePlugin(PluginType pluginType, std::unique_ptr<PluginConfig> conf);

    bool CheckPluginRunning(PluginType pluginType);

    int32_t PollPerfBuffers(PluginType, int32_t, int32_t*, int);

    bool SetNetworkObserverConfig(int32_t key, int32_t value);
    bool SetNetworkObserverCidFilter(const std::string&, bool update);

    // for bpf object operations ...
    bool BPFMapUpdateElem(PluginType pluginType, const std::string& mapName, void* key, void* value, uint64_t flag);

    EBPFAdapter();
    ~EBPFAdapter();

private:
    bool loadDynamicLib(const std::string& libName);
    bool loadCoolBPF();
    bool dynamicLibSuccess();

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
        EBPF_SET_NETWORKOBSERVER_CONFIG,
        EBPF_SET_NETWORKOBSERVER_CID_FILTER,

        // operations
        EBPF_MAP_UPDATE_ELEM,
        EBPF_FUNC_MAX,
    };

    std::shared_ptr<DynamicLibLoader> mLib;
    std::shared_ptr<DynamicLibLoader> mCoolbpfLib;
    std::array<void*, (int)ebpf_func::EBPF_FUNC_MAX> mFuncs = {};
    std::array<long, (int)network_observer_uprobe_funcs::EBPF_NETWORK_OBSERVER_MAX> mOffsets = {};
    std::array<std::atomic_bool, (int)PluginType::MAX> mRunning = {};
    std::string mBinaryPath;
    std::string mFullLibName;

    eBPFLogHandler mLogPrinter;

#ifdef APSARA_UNIT_TEST_MAIN
    std::unique_ptr<PluginConfig> mConfig;
    friend class eBPFServerUnittest;
#endif
};

} // namespace logtail::ebpf
