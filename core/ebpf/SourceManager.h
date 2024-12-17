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

#include <dlfcn.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <array>

#include "ebpf/include/export.h"
#include "ebpf/include/SysAkApi.h"
#include "common/DynamicLibHelper.h"

namespace logtail {
namespace ebpf {

enum class eBPFPluginType {
  SOCKETTRACE = 0,
  PROCESS = 1,
  MAX = 2,
};

class SourceManager {
public:
    const std::string m_lib_name_ = "network_observer";
    
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

    std::shared_ptr<DynamicLibLoader> mLib;
    std::array<void*, (int)ebpf_func::EBPF_MAX> mFuncs = {};
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

}
}
