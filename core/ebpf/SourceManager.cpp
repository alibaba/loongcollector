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

#include "ebpf/SourceManager.h"

#include <filesystem>
#include <memory>
#include <string>

#include "app_config/AppConfig.h"
#include "common/Flags.h"
#include "common/LogtailCommonFlags.h"
#include "common/MachineInfoUtil.h"
#include "common/RuntimeUtil.h"
#include "ebpf/driver/NetworkObserver.h"
#include "ebpf/include/SysAkApi.h"
#include "logger/Logger.h"

namespace logtail {
namespace ebpf {

#define LOAD_EBPF_FUNC_ADDR(funcName) \
    ({ \
        void* funcPtr = tmp_lib->LoadMethod(#funcName, loadErr); \
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
        void* funcPtr = tmp_lib->LoadMethod(#funcName, loadErr); \
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


SourceManager::SourceManager() = default;

SourceManager::~SourceManager() {
    if (!DynamicLibSuccess()) {
        return;
    }

    for (size_t i = 0; i < mRunning.size(); i++) {
        auto& x = mRunning[i];
        if (!x) {
            continue;
        }
        // stop plugin
        StopPlugin(static_cast<PluginType>(i));
    }

#ifdef APSARA_UNIT_TEST_MAIN
    return;
#endif

    // call deinit
    // void* f = mFuncs[(int)ebpf_func::EBPF_DEINIT];
    // if (!f) {
    //     return;
    // }

    // auto deinit_f = (deinit_func)f;
    // deinit_f();
}

void SourceManager::Init() {
    mHostIp = GetHostIp();
    mHostName = GetHostName();

    // read host path prefix
    if (AppConfig::GetInstance()->IsPurageContainerMode()) {
        mHostPathPrefix = STRING_FLAG(default_container_host_path);
        LOG_DEBUG(sLogger, ("running in container mode, would set host path prefix to ", mHostPathPrefix));
    } else {
        LOG_DEBUG(sLogger, ("running in host mode", "would not set host path prefix ..."));
    }

    mBinaryPath = GetProcessExecutionDir();
    mFullLibName = "lib" + mDriverLibName + ".so";
    for (auto& x : mRunning) {
        x = false;
    }

    mLogPrinter = [](int16_t level, const char* format, va_list args) {
        eBPFLogType printLevel = (eBPFLogType)level;
        char buffer[1024] = {0};
        vsnprintf(buffer, 1023, format, args);
        switch (printLevel) {
            case eBPFLogType::NAMI_LOG_TYPE_WARN:
                LOG_WARNING(sLogger, ("module", "ebpf_driver")("msg", buffer));
                break;
            case eBPFLogType::NAMI_LOG_TYPE_INFO:
                LOG_INFO(sLogger, ("module", "ebpf_driver")("msg", buffer));
                break;
            case eBPFLogType::NAMI_LOG_TYPE_DEBUG:
                LOG_DEBUG(sLogger, ("module", "ebpf_driver")("msg", buffer));
                break;
            default:
                LOG_INFO(sLogger, ("module", "ebpf_driver")("level", int(level))("msg", buffer));
                break;
        }
        return 0;
    };
}

bool SourceManager::LoadDynamicLib(const std::string& lib_name) {
    if (DynamicLibSuccess()) {
        // already load
        return true;
    }

    std::shared_ptr<DynamicLibLoader> tmp_lib = std::make_shared<DynamicLibLoader>();
    LOG_INFO(sLogger, ("[SourceManager] begin load ebpf dylib, path:", mBinaryPath));
    std::string loadErr;
    if (!tmp_lib->LoadDynLib(lib_name, loadErr, mBinaryPath)) {
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
    mFuncs[static_cast<int>(ebpf_func::EBPF_SET_NETWORKOBSERVER_CONFIG)]
        = LOAD_EBPF_FUNC_ADDR(set_networkobserver_config);
    mFuncs[static_cast<int>(ebpf_func::EBPF_SET_NETWORKOBSERVER_CID_FILTER)]
        = LOAD_EBPF_FUNC_ADDR(set_networkobserver_cid_filter);
    mFuncs[static_cast<int>(ebpf_func::EBPF_MAP_UPDATE_ELEM)] = LOAD_EBPF_FUNC_ADDR(update_bpf_map_elem);

    // check function load success
    if (std::any_of(mFuncs.begin(), mFuncs.end(), [](auto* x) { return x == nullptr; })) {
        return false;
    }

    // load offset ...
    if (!LoadCoolBPF()) {
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
    mLib = std::move(tmp_lib);
    return true;
}

bool SourceManager::LoadCoolBPF() {
    if (DynamicLibSuccess()) {
        // already load
        return true;
    }

    std::shared_ptr<DynamicLibLoader> tmp_lib = std::make_shared<DynamicLibLoader>();
    LOG_INFO(sLogger, ("[SourceManager] begin load libcoolbpf, path:", mBinaryPath));
    std::string loadErr;
    if (!tmp_lib->LoadDynLib("coolbpf", loadErr, mBinaryPath, ".1.0.0")) {
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
    if (!std::all_of(mOffsets.begin(), mOffsets.end(), [](auto x) { return x > 0; })) {
        LOG_ERROR(sLogger, ("failed to load libcoolbpf funcs addr, path", mBinaryPath));
        return false;
    }

    mCoolbpfLib = std::move(tmp_lib);

    return true;
}

bool SourceManager::DynamicLibSuccess() {
    // #ifdef APSARA_UNIT_TEST_MAIN
    //     return true;
    // #endif
    if (!mLib)
        return false;
    if (!std::all_of(mFuncs.begin(), mFuncs.end(), [](auto* x) { return x != nullptr; })) {
        return false;
    }
    return true;
}

bool SourceManager::CheckPluginRunning(PluginType plugin_type) {
    if (!LoadDynamicLib(mDriverLibName)) {
        LOG_ERROR(sLogger, ("dynamic lib not load, plugin type:", int(plugin_type)));
        return false;
    }

    return mRunning[int(plugin_type)];
}

bool SourceManager::SetNetworkObserverConfig(int32_t key, int32_t value) {
    if (!DynamicLibSuccess()) {
        return false;
    }
    void* f = mFuncs[static_cast<int>(ebpf_func::EBPF_SET_NETWORKOBSERVER_CONFIG)];
    if (!f) {
        LOG_ERROR(sLogger, ("failed to load dynamic lib, set networkobserver config func ptr is null", ""));
        return false;
    }

    auto func = (set_networkobserver_config_func)f;
    func(key, value);
    return true;
}

bool SourceManager::SetNetworkObserverCidFilter(const std::string& cid, bool update) {
    if (!DynamicLibSuccess()) {
        return false;
    }
    void* f = mFuncs[static_cast<int>(ebpf_func::EBPF_SET_NETWORKOBSERVER_CID_FILTER)];
    if (!f) {
        LOG_ERROR(sLogger, ("failed to load dynamic lib, set networkobserver config func ptr is null", ""));
        return false;
    }

    auto func = (set_networkobserver_cid_filter_func)f;
    func(cid.c_str(), cid.size(), update);
    return true;
}

int32_t SourceManager::PollPerfBuffers(PluginType plugin_type, int32_t maxEvents, int32_t* flag, int timeoutMs) {
    if (!DynamicLibSuccess()) {
        return -1;
    }
    void* f = mFuncs[static_cast<int>(ebpf_func::EBPF_POLL_PLUGIN_PBS)];
    if (!f) {
        LOG_ERROR(sLogger, ("failed to load dynamic lib, poll perf buffer func ptr is null", int(plugin_type)));
        return -1;
    }

    auto poll_func = (poll_plugin_pbs_func)f;
    return poll_func(plugin_type, maxEvents, flag, timeoutMs);
}

bool SourceManager::StartPlugin(PluginType plugin_type, std::unique_ptr<PluginConfig> conf) {
    if (CheckPluginRunning(plugin_type)) {
        // plugin update ...
        return UpdatePlugin(plugin_type, std::move(conf));
    }

    // plugin not started ...
    LOG_INFO(sLogger, ("begin to start plugin, type", int(plugin_type)));
    if (conf->mPluginType == PluginType::NETWORK_OBSERVE) {
        auto nconf = std::get_if<NetworkObserveConfig>(&conf->mConfig);
        if (nconf) {
            nconf->mSo = mBinaryPath + "libcoolbpf.so.1.0.0";
            nconf->mLogHandler = mLogPrinter;
            nconf->mUpcaOffset
                = mOffsets[static_cast<int>(network_observer_uprobe_funcs::EBPF_NETWORK_OBSERVER_UPDATE_CONN_ADDR)];
            nconf->mUprobeOffset
                = mOffsets[static_cast<int>(network_observer_uprobe_funcs::EBPF_NETWORK_OBSERVER_CLEAN_UP_DOG)];
            nconf->mUpcrOffset
                = mOffsets[static_cast<int>(network_observer_uprobe_funcs::EBPF_NETWORK_OBSERVER_UPDATE_CONN_ROLE)];
            nconf->mUppsOffset
                = mOffsets[static_cast<int>(network_observer_uprobe_funcs::EBPF_NETWORK_OBSERVER_DISABLE_PROCESS)];
        }
    }

    void* f = mFuncs[(int)ebpf_func::EBPF_START_PLUGIN];
    if (!f) {
        LOG_ERROR(sLogger, ("failed to load dynamic lib, init func ptr is null", int(plugin_type)));
        return false;
    }
    auto start_f = (start_plugin_func)f;
    int res = start_f(conf.get());
    if (!res)
        mRunning[int(plugin_type)] = true;
    return !res;
}

bool SourceManager::ResumePlugin(PluginType plugin_type, std::unique_ptr<PluginConfig> conf) {
    if (!CheckPluginRunning(plugin_type)) {
        LOG_ERROR(sLogger, ("plugin not started, type", int(plugin_type)));
        return false;
    }

    LOG_INFO(sLogger, ("begin to resume plugin, type", int(plugin_type)));
    void* f = mFuncs[(int)ebpf_func::EBPF_RESUME_PLUGIN];
    if (!f) {
        LOG_ERROR(sLogger, ("failed to load dynamic lib, update func ptr is null", int(plugin_type)));
        return false;
    }

    auto resume_f = (resume_plugin_func)f;
    int res = resume_f(conf.get());
    return !res;
}

bool SourceManager::UpdatePlugin(PluginType plugin_type, std::unique_ptr<PluginConfig> conf) {
    if (!CheckPluginRunning(plugin_type)) {
        LOG_ERROR(sLogger, ("plugin not started, type", int(plugin_type)));
        return false;
    }

    LOG_INFO(sLogger, ("begin to update plugin, type", int(plugin_type)));
    void* f = mFuncs[(int)ebpf_func::EBPF_UPDATE_PLUGIN];
    if (!f) {
        LOG_ERROR(sLogger, ("failed to load dynamic lib, update func ptr is null", int(plugin_type)));
        return false;
    }

    auto update_f = (update_plugin_func)f;
    int res = update_f(conf.get());
    return !res;
}

bool SourceManager::SuspendPlugin(PluginType plugin_type) {
    if (!CheckPluginRunning(plugin_type)) {
        LOG_WARNING(sLogger, ("plugin not started, cannot suspend. type", int(plugin_type)));
        return false;
    }
    void* f = mFuncs[(int)ebpf_func::EBPF_SUSPEND_PLUGIN];
    if (!f) {
        LOG_ERROR(sLogger, ("failed to load dynamic lib, suspend func ptr is null", int(plugin_type)));
        return false;
    }

    auto suspend_f = (suspend_plugin_func)f;
    int res = suspend_f(plugin_type);

    return !res;
}

bool SourceManager::StopPlugin(PluginType plugin_type) {
    if (!CheckPluginRunning(plugin_type)) {
        LOG_WARNING(sLogger, ("plugin not started, do nothing. type", int(plugin_type)));
        return true;
    }

    auto config = std::make_unique<PluginConfig>();
    config->mPluginType = plugin_type;
    void* f = mFuncs[(int)ebpf_func::EBPF_STOP_PLUGIN];
    if (!f) {
        LOG_ERROR(sLogger, ("failed to load dynamic lib, stop func ptr is null", int(plugin_type)));
        return false;
    }

    auto stop_f = (stop_plugin_func)f;
    int res = stop_f(plugin_type);
    if (!res)
        mRunning[int(plugin_type)] = false;
    return !res;
}

bool SourceManager::BPFMapUpdateElem(
    PluginType plugin_type, const std::string& map_name, void* key, void* value, uint64_t flag) {
    if (!CheckPluginRunning(plugin_type)) {
        LOG_WARNING(sLogger, ("plugin not started, do nothing. type", int(plugin_type)));
        return true;
    }

    void* f = mFuncs[(int)ebpf_func::EBPF_MAP_UPDATE_ELEM];
    if (!f) {
        LOG_ERROR(sLogger, ("failed to load dynamic lib, update bpf map elem func ptr is null", int(plugin_type)));
        return false;
    }

    auto ff = (update_bpf_map_elem_func)f;
    int res = ff(plugin_type, map_name.c_str(), key, value, flag);
    return res == 0;
}

} // namespace ebpf
} // namespace logtail
