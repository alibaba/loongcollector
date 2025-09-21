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

#include "ebpf/EBPFAdapter.h"

#include <cstdlib>

#include <memory>
#include <string>

#include "spdlog/common.h"

#include "common/RuntimeUtil.h"
#include "common/magic_enum.hpp"
#include "ebpf/driver/eBPFDriver.h"
#include "logger/Logger.h"

namespace logtail::ebpf {

#define LOAD_EBPF_FUNC_ADDR(funcName) \
    ({ \
        void* funcPtr = tmpLib->LoadMethod(#funcName, loadErr); \
        if (funcPtr == NULL) { \
            LOG_ERROR(sLogger, \
                      ("[source_manager] load ebpf method", "failed")("method", #funcName)("error", loadErr)); \
        } \
        funcPtr; \
    })

#define LOAD_UPROBE_OFFSET_FAIL (-100)
#define LOAD_UPROBE_OFFSET(funcAddr) \
    ({ \
        Dl_info dlinfo; \
        int dlAddrErr = dladdr((const void*)(funcAddr), &dlinfo); \
        long res = 0; \
        if (dlAddrErr == 0) { \
            LOG_ERROR(sLogger, \
                      ("[source_manager] load ebpf dl address", "failed")("error", dlAddrErr)("method", #funcAddr)); \
            res = LOAD_UPROBE_OFFSET_FAIL; \
        } else { \
            res = (long)dlinfo.dli_saddr - (long)dlinfo.dli_fbase; \
            LOG_DEBUG(sLogger, ("func", #funcAddr)("offset", res)); \
        } \
        res; \
    })

#define LOAD_EBPF_FUNC_AND_UPROBE_OFFSET(funcName) \
    ({ \
        void* funcPtr = tmpLib->LoadMethod(#funcName, loadErr); \
        long offset = 0; \
        if (funcPtr == NULL) { \
            LOG_ERROR(sLogger, \
                      ("[source_manager] load ebpf method", "failed")("method", #funcName)("error", loadErr)); \
            offset = LOAD_UPROBE_OFFSET_FAIL; \
        } else { \
            Dl_info dlinfo; \
            int dlAddrErr = dladdr((const void*)(funcPtr), &dlinfo); \
            if (dlAddrErr == 0) { \
                LOG_ERROR( \
                    sLogger, \
                    ("[source_manager] load ebpf dl address", "failed")("error", dlAddrErr)("method", #funcName)); \
                offset = LOAD_UPROBE_OFFSET_FAIL; \
            } else { \
                offset = (long)dlinfo.dli_saddr - (long)dlinfo.dli_fbase; \
                LOG_DEBUG(sLogger, ("func", #funcName)("offset", offset)); \
            } \
        } \
        offset; \
    })


EBPFAdapter::EBPFAdapter() = default;

EBPFAdapter::~EBPFAdapter() {
    // Plugin cleanup is handled by EBPFServer::Stop() to avoid
    // accessing potentially freed resources during global destruction
}

void EBPFAdapter::Init() {
    if (mInited) {
        return;
    }
    mInited = true;
    mBinaryPath = GetProcessExecutionDir();
    setenv("SYSAK_WORK_PATH", mBinaryPath.c_str(), 1);

    mLogPrinter = [](int16_t level, const char* format, va_list args) -> int {
        auto printLevel = (eBPFLogType)level;
        switch (printLevel) {
            case eBPFLogType::NAMI_LOG_TYPE_WARN:
                if (!SHOULD_LOG_WARNING(sLogger)) {
                    return 0;
                }
                break;
            case eBPFLogType::NAMI_LOG_TYPE_DEBUG:
                if (!SHOULD_LOG_DEBUG(sLogger)) {
                    return 0;
                }
                break;
            case eBPFLogType::NAMI_LOG_TYPE_INFO:
                [[fallthrough]];
            default:
                if (!SHOULD_LOG_ERROR(sLogger)) {
                    return 0;
                }
                break;
        }
        char buffer[4096] = {0};
        vsnprintf(buffer, sizeof(buffer), format, args);
        buffer[sizeof(buffer) - 1] = '\0';
        switch (printLevel) {
            case eBPFLogType::NAMI_LOG_TYPE_WARN:
                sLogger->log(spdlog::level::warn, "{}", buffer);
                break;
            case eBPFLogType::NAMI_LOG_TYPE_INFO:
                sLogger->log(spdlog::level::info, "{}", buffer);
                break;
            case eBPFLogType::NAMI_LOG_TYPE_DEBUG:
                sLogger->log(spdlog::level::debug, "{}", buffer);
                break;
            default:
                sLogger->log(spdlog::level::err, "{}", buffer);
                break;
        }
        return 0;
    };
}

bool EBPFAdapter::loadDynamicLib(const std::string& libName) {
    if (dynamicLibSuccess()) {
        // already load
        return true;
    }

    std::shared_ptr<DynamicLibLoader> tmpLib = std::make_shared<DynamicLibLoader>();
    LOG_INFO(sLogger, ("[EBPFAdapter] begin load ebpf dylib, path", mBinaryPath));
    std::string loadErr;
    if (!tmpLib->LoadDynLib(libName, loadErr, mBinaryPath)) {
        LOG_ERROR(sLogger, ("failed to load ebpf dynamic library, path", mBinaryPath)("error", loadErr));
        return false;
    }

    // load method
    mFuncs[static_cast<int>(ebpf_func::EBPF_SET_LOGGER)] = LOAD_EBPF_FUNC_ADDR(set_logger);
    mFuncs[static_cast<int>(ebpf_func::EBPF_START_PLUGIN)] = LOAD_EBPF_FUNC_ADDR(start_plugin);
    mFuncs[static_cast<int>(ebpf_func::EBPF_UPDATE_PLUGIN)] = LOAD_EBPF_FUNC_ADDR(update_plugin);
    mFuncs[static_cast<int>(ebpf_func::EBPF_STOP_PLUGIN)] = LOAD_EBPF_FUNC_ADDR(stop_plugin);
    mFuncs[static_cast<int>(ebpf_func::EBPF_SUSPEND_PLUGIN)] = LOAD_EBPF_FUNC_ADDR(suspend_plugin);
    mFuncs[static_cast<int>(ebpf_func::EBPF_RESUME_PLUGIN)] = LOAD_EBPF_FUNC_ADDR(resume_plugin);
    mFuncs[static_cast<int>(ebpf_func::EBPF_POLL_PLUGIN_PBS)] = LOAD_EBPF_FUNC_ADDR(poll_plugin_pbs);
    mFuncs[static_cast<int>(ebpf_func::EBPF_CONSUME_PLUGIN_PB_DATA)] = LOAD_EBPF_FUNC_ADDR(consume_plugin_pb_data);
    mFuncs[static_cast<int>(ebpf_func::EBPF_SET_NETWORKOBSERVER_CONFIG)]
        = LOAD_EBPF_FUNC_ADDR(set_networkobserver_config);
    mFuncs[static_cast<int>(ebpf_func::EBPF_SET_NETWORKOBSERVER_CID_FILTER)]
        = LOAD_EBPF_FUNC_ADDR(set_networkobserver_cid_filter);
    mFuncs[static_cast<int>(ebpf_func::EBPF_MAP_UPDATE_ELEM)] = LOAD_EBPF_FUNC_ADDR(update_bpf_map_elem);
    mFuncs[static_cast<int>(ebpf_func::EBPF_GET_PLUGIN_PB_EPOLL_FDS)] = LOAD_EBPF_FUNC_ADDR(get_plugin_pb_epoll_fds);

    // check function load success
    if (std::any_of(mFuncs.begin(), mFuncs.end(), [](auto* x) { return x == nullptr; })) {
        return false;
    }

    // load offset ...
    if (!loadCoolBPF()) {
        LOG_ERROR(sLogger, ("failed to load coolbpf", ""));
        return false;
    }

    // set global logger ...
    auto eBPFSetLogger = (set_logger_func)mFuncs[static_cast<int>(ebpf_func::EBPF_SET_LOGGER)];
    if (!eBPFSetLogger) {
        LOG_WARNING(
            sLogger,
            ("cannot set logger for ebpf driver, because set_logger func was load incorrectly ... ", "please check"));
    } else {
        eBPFSetLogger(mLogPrinter);
    }

    // update meta
    mLib = std::move(tmpLib);
    return true;
}

bool EBPFAdapter::loadCoolBPF() {
    if (dynamicLibSuccess()) {
        // already load
        return true;
    }

    std::shared_ptr<DynamicLibLoader> tmpLib = std::make_shared<DynamicLibLoader>();
    LOG_INFO(sLogger, ("[EBPFAdapter] begin load libcoolbpf, path", mBinaryPath));
    std::string loadErr;
    if (!tmpLib->LoadDynLib("coolbpf", loadErr, mBinaryPath, ".1.0.0")) {
        LOG_ERROR(sLogger, ("failed to load libcoolbpf, path", mBinaryPath)("error", loadErr));
        return false;
    }

    // load address
    mOffsets[static_cast<int>(network_observer_uprobe_funcs::EBPF_NETWORK_OBSERVER_CLEAN_UP_DOG)]
        = LOAD_EBPF_FUNC_AND_UPROBE_OFFSET(ebpf_cleanup_dog);
    mOffsets[static_cast<int>(network_observer_uprobe_funcs::EBPF_NETWORK_OBSERVER_UPDATE_CONN_ROLE)]
        = LOAD_EBPF_FUNC_AND_UPROBE_OFFSET(ebpf_update_conn_role);
    mOffsets[static_cast<int>(network_observer_uprobe_funcs::EBPF_NETWORK_OBSERVER_DISABLE_PROCESS)]
        = LOAD_EBPF_FUNC_AND_UPROBE_OFFSET(ebpf_disable_process);
    mOffsets[static_cast<int>(network_observer_uprobe_funcs::EBPF_NETWORK_OBSERVER_UPDATE_CONN_ADDR)]
        = LOAD_EBPF_FUNC_AND_UPROBE_OFFSET(ebpf_update_conn_addr);
    mOffsets[static_cast<int>(network_observer_uprobe_funcs::EBPF_NETWORK_OBSERVER_GET_RUNTIME_INFO)]
        = LOAD_EBPF_FUNC_AND_UPROBE_OFFSET(get_self_runtime_info);
    if (!std::all_of(mOffsets.begin(), mOffsets.end(), [](auto x) { return x > 0; })) {
        LOG_ERROR(sLogger, ("failed to load libcoolbpf funcs addr, path", mBinaryPath));
        return false;
    }

    mCoolbpfLib = std::move(tmpLib);

    return true;
}

bool EBPFAdapter::dynamicLibSuccess() {
    // #ifdef APSARA_UNIT_TEST_MAIN
    //     return true;
    // #endif
    if (!mLib) {
        return false;
    }
    if (!std::all_of(mFuncs.begin(), mFuncs.end(), [](auto* x) { return x != nullptr; })) {
        return false;
    }
    return true;
}


bool EBPFAdapter::SetNetworkObserverConfig(int32_t key, int32_t value) {
    if (!dynamicLibSuccess()) {
        return false;
    }
    void* f = mFuncs[static_cast<int>(ebpf_func::EBPF_SET_NETWORKOBSERVER_CONFIG)];
    if (!f) {
        LOG_ERROR(sLogger, ("failed to load dynamic lib, set networkobserver config func ptr is null", ""));
        return false;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    return true;
#else
    auto func = (set_networkobserver_config_func)f;
    func(key, value);
    return true;
#endif
}

bool EBPFAdapter::SetNetworkObserverCidFilter(const std::string& cid, bool update, uint64_t cidKey) {
    if (!dynamicLibSuccess()) {
        return false;
    }
    void* f = mFuncs[static_cast<int>(ebpf_func::EBPF_SET_NETWORKOBSERVER_CID_FILTER)];
    if (!f) {
        LOG_ERROR(sLogger, ("failed to load dynamic lib, set networkobserver config func ptr is null", ""));
        return false;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    return true;
#else
    auto func = (set_networkobserver_cid_filter_func)f;
    func(cid.c_str(), cid.size(), cidKey, update);
    return true;
#endif
}

int32_t EBPFAdapter::PollPerfBuffers(PluginType pluginType, int32_t maxEvents, int32_t* flag, int timeoutMs) {
    if (!dynamicLibSuccess()) {
        return -1;
    }
    void* f = mFuncs[static_cast<int>(ebpf_func::EBPF_POLL_PLUGIN_PBS)];
    if (!f) {
        LOG_ERROR(sLogger,
                  ("failed to load dynamic lib, poll perf buffer func ptr is null", magic_enum::enum_name(pluginType)));
        return -1;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    return 0;
#else
    auto pollFunc = (poll_plugin_pbs_func)f;
    return pollFunc(pluginType, maxEvents, flag, timeoutMs);
#endif
}

bool EBPFAdapter::StartPlugin(PluginType pluginType, std::unique_ptr<PluginConfig> conf) {
    if (!loadDynamicLib(mDriverLibName)) {
        LOG_ERROR(sLogger, ("dynamic lib not load, plugin type", magic_enum::enum_name(pluginType)));
        return false;
    }

    // Plugin state is managed by upper layer, directly execute start logic
    LOG_INFO(sLogger, ("begin to start plugin, type", magic_enum::enum_name(pluginType)));
    if (conf->mPluginType == PluginType::NETWORK_OBSERVE) {
        auto* nconf = std::get_if<NetworkObserveConfig>(&conf->mConfig);
        if (nconf) {
            nconf->mSo = mBinaryPath + "libcoolbpf.so.1.0.0";
            nconf->mLogHandler = mLogPrinter; // TODO: unneccessary
            nconf->mUpcaOffset
                = mOffsets[static_cast<int>(network_observer_uprobe_funcs::EBPF_NETWORK_OBSERVER_UPDATE_CONN_ADDR)];
            nconf->mUprobeOffset
                = mOffsets[static_cast<int>(network_observer_uprobe_funcs::EBPF_NETWORK_OBSERVER_CLEAN_UP_DOG)];
            nconf->mUpcrOffset
                = mOffsets[static_cast<int>(network_observer_uprobe_funcs::EBPF_NETWORK_OBSERVER_UPDATE_CONN_ROLE)];
            nconf->mUppsOffset
                = mOffsets[static_cast<int>(network_observer_uprobe_funcs::EBPF_NETWORK_OBSERVER_DISABLE_PROCESS)];
            nconf->mUpgsOffset
                = mOffsets[static_cast<int>(network_observer_uprobe_funcs::EBPF_NETWORK_OBSERVER_GET_RUNTIME_INFO)];
        }
    }

    void* f = mFuncs[(int)ebpf_func::EBPF_START_PLUGIN];
    if (!f) {
        LOG_ERROR(sLogger, ("failed to load dynamic lib, init func ptr is null", magic_enum::enum_name(pluginType)));
        return false;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    return true;
#else
    auto startF = (start_plugin_func)f;
    int res = startF(conf.get());
    return !res;
#endif
}

bool EBPFAdapter::ResumePlugin(PluginType pluginType, std::unique_ptr<PluginConfig> conf) {
    if (!loadDynamicLib(mDriverLibName)) {
        LOG_ERROR(sLogger, ("dynamic lib not load, plugin type", magic_enum::enum_name(pluginType)));
        return false;
    }

    LOG_INFO(sLogger, ("begin to resume plugin, type", magic_enum::enum_name(pluginType)));
    void* f = mFuncs[(int)ebpf_func::EBPF_RESUME_PLUGIN];
    if (!f) {
        LOG_ERROR(sLogger, ("failed to load dynamic lib, update func ptr is null", magic_enum::enum_name(pluginType)));
        return false;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    return true;
#else
    auto resumeF = (resume_plugin_func)f;
    int res = resumeF(conf.get());
    return !res;
#endif
}

bool EBPFAdapter::UpdatePlugin(PluginType pluginType, std::unique_ptr<PluginConfig> conf) {
    if (!loadDynamicLib(mDriverLibName)) {
        LOG_ERROR(sLogger, ("dynamic lib not load, plugin type", magic_enum::enum_name(pluginType)));
        return false;
    }

    LOG_INFO(sLogger, ("begin to update plugin, type", magic_enum::enum_name(pluginType)));
    void* f = mFuncs[(int)ebpf_func::EBPF_UPDATE_PLUGIN];
    if (!f) {
        LOG_ERROR(sLogger, ("failed to load dynamic lib, update func ptr is null", magic_enum::enum_name(pluginType)));
        return false;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    return true;
#else
    auto updateF = (update_plugin_func)f;
    int res = updateF(conf.get());
    return !res;
#endif
}

bool EBPFAdapter::SuspendPlugin(PluginType pluginType) {
    if (!loadDynamicLib(mDriverLibName)) {
        LOG_ERROR(sLogger, ("dynamic lib not load, plugin type", magic_enum::enum_name(pluginType)));
        return false;
    }

    void* f = mFuncs[(int)ebpf_func::EBPF_SUSPEND_PLUGIN];
    if (!f) {
        LOG_ERROR(sLogger, ("failed to load dynamic lib, suspend func ptr is null", magic_enum::enum_name(pluginType)));
        return false;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    return true;
#else
    auto suspendF = (suspend_plugin_func)f;
    int res = suspendF(pluginType);

    return !res;
#endif
}

bool EBPFAdapter::StopPlugin(PluginType pluginType) {
    if (!loadDynamicLib(mDriverLibName)) {
        LOG_ERROR(sLogger, ("dynamic lib not load, plugin type", magic_enum::enum_name(pluginType)));
        return false;
    }

    auto config = std::make_unique<PluginConfig>();
    config->mPluginType = pluginType;
    void* f = mFuncs[(int)ebpf_func::EBPF_STOP_PLUGIN];
    if (!f) {
        LOG_ERROR(sLogger, ("failed to load dynamic lib, stop func ptr is null", magic_enum::enum_name(pluginType)));
        return false;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    return true;
#else
    auto stopF = (stop_plugin_func)f;
    int res = stopF(pluginType);
    return !res;
#endif
}

bool EBPFAdapter::BPFMapUpdateElem(
    PluginType pluginType, const std::string& map_name, void* key, void* value, uint64_t flag) {
    if (!loadDynamicLib(mDriverLibName)) {
        LOG_ERROR(sLogger, ("dynamic lib not load, plugin type", magic_enum::enum_name(pluginType)));
        return false;
    }

    void* f = mFuncs[(int)ebpf_func::EBPF_MAP_UPDATE_ELEM];
    if (!f) {
        LOG_ERROR(
            sLogger,
            ("failed to load dynamic lib, update bpf map elem func ptr is null", magic_enum::enum_name(pluginType)));
        return false;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    return true;
#else
    auto ff = (update_bpf_map_elem_func)f;
    int res = ff(pluginType, map_name.c_str(), key, value, flag);
    return res == 0;
#endif
}

std::vector<int> EBPFAdapter::GetPerfBufferEpollFds(PluginType pluginType) {
    if (!dynamicLibSuccess()) {
        return {};
    }
    void* f = mFuncs[static_cast<int>(ebpf_func::EBPF_GET_PLUGIN_PB_EPOLL_FDS)];
    if (!f) {
        LOG_ERROR(sLogger,
                  ("failed to load dynamic lib, get perf buffer epoll fds func ptr is null",
                   magic_enum::enum_name(pluginType)));
        return {};
    }
#ifdef APSARA_UNIT_TEST_MAIN
    return {0, 1, 2};
#else
    auto getEpollFdsFunc = (get_plugin_pb_epoll_fds_func)f;
    static const int kMaxFds = 8;
    int fds[kMaxFds];
    int count = getEpollFdsFunc(pluginType, static_cast<int*>(fds), kMaxFds);

    std::vector<int> result;
    if (count > 0) {
        result.assign(static_cast<int*>(fds), static_cast<int*>(fds) + count);
    }
    return result;
#endif
}

int32_t EBPFAdapter::ConsumePerfBufferData(PluginType pluginType) {
    if (!dynamicLibSuccess()) {
        return -1;
    }
    void* f = mFuncs[static_cast<int>(ebpf_func::EBPF_CONSUME_PLUGIN_PB_DATA)];
    if (!f) {
        LOG_ERROR(sLogger,
                  ("failed to load dynamic lib, consume perf buffer data func ptr is null",
                   magic_enum::enum_name(pluginType)));
        return -1;
    }
#ifdef APSARA_UNIT_TEST_MAIN
    return 0;
#else
    auto consumeFunc = (consume_plugin_pb_data_func)f;
    return consumeFunc(pluginType);
#endif
}

} // namespace logtail::ebpf
