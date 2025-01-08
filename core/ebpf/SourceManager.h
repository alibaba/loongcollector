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

    bool SetNetworkObserverConfig(int32_t key, int32_t value);
    bool SetNetworkObserverCidFilter(const std::string&, bool update);

    SourceManager();
    ~SourceManager();

private:
    bool LoadDynamicLib(const std::string& lib_name);
    bool LoadCoolBPF();
    bool DynamicLibSuccess();
    bool UpdatePlugin(logtail::ebpf::PluginType plugin_type, std::unique_ptr<logtail::ebpf::PluginConfig> conf);

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
        EBPF_FUNC_MAX,
    };

    std::shared_ptr<DynamicLibLoader> mLib;
    std::shared_ptr<DynamicLibLoader> mCoolbpfLib;
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
