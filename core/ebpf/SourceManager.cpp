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
#include "common/RuntimeUtil.h"
#include "logger/Logger.h"
#include "ebpf/include/SysAkApi.h"
#include "common/MachineInfoUtil.h"
#include "common/LogtailCommonFlags.h"

#include <string>
#include <filesystem>
#include <memory>
#include <gflags/gflags.h>

namespace logtail{
namespace ebpf {

#define LOAD_EBPF_FUNC_ADDR(funcName) \
    ({ \
        void* funcPtr = tmp_lib->LoadMethod(#funcName, loadErr); \
        if (funcPtr == NULL) { \
            LOG_ERROR(sLogger, ("[source_manager] load ebpf method", "failed")("method", #funcName)("error", loadErr)); \
        } \
        funcPtr; \
    })

#define LOAD_UPROBE_OFFSET_FAIL (-100)
#define LOAD_UPROBE_OFFSET(funcName) \
    ({ \
        Dl_info dlinfo; \
        int dlAddrErr = dladdr((const void*)(funcName), &dlinfo); \
        long res = 0; \
        if (dlAddrErr == 0) { \
            LOG_ERROR(sLogger, \
                      ("[source_manager] load ebpf dl address", "failed")("error", dlAddrErr)("method", #funcName)); \
            res = LOAD_UPROBE_OFFSET_FAIL; \
        } else { \
            res = (long)dlinfo.dli_saddr - (long)dlinfo.dli_fbase; \
            LOG_DEBUG(sLogger, ("func", #funcName)("offset", res)); \
        } \
        res; \
    })
    
SourceManager::SourceManager() {
}

SourceManager::~SourceManager() {
}

void SourceManager::Init() {
  mHostIp = GetHostIp();
  mHostName = GetHostName();
  mHostPathPrefix = STRING_FLAG(default_container_host_path);
  mBinaryPath = GetProcessExecutionDir();
  mFullLibName = "lib" + m_lib_name_ + ".so";
  for (auto& x : mRunning) {
    x = false;
  }
}

bool SourceManager::LoadDynamicLib(const std::string& lib_name) {
  if (DynamicLibSuccess()) {
    // already load
    return true;
  }
#ifdef APSARA_UNIT_TEST_MAIN
    return true;
#endif
  std::shared_ptr<DynamicLibLoader> tmp_lib = std::make_shared<DynamicLibLoader>();
  LOG_INFO(sLogger, ("[SourceManager] begin load ebpf dylib, path:", mBinaryPath));
  std::string loadErr;
  if (!tmp_lib->LoadDynLib(lib_name, loadErr, mBinaryPath)) {
      LOG_ERROR(sLogger, ("failed to load ebpf dynamic library, path", mBinaryPath)("error", loadErr));
      return false;
  }
  // init funcs
  // set offset configs
  // init config
  // this memory will be managed by plugin

  mFuncs[(int)ebpf_func::EBPF_INIT] = LOAD_EBPF_FUNC_ADDR(init);
  mFuncs[(int)ebpf_func::EBPF_UPDATE] = LOAD_EBPF_FUNC_ADDR(update);
  mFuncs[(int)ebpf_func::EBPF_SUSPEND] = LOAD_EBPF_FUNC_ADDR(suspend);
  mFuncs[(int)ebpf_func::EBPF_DEINIT] = LOAD_EBPF_FUNC_ADDR(deinit);
  mFuncs[(int)ebpf_func::EBPF_REMOVE] = LOAD_EBPF_FUNC_ADDR(removep);
  mFuncs[(int)ebpf_func::EBPF_SOCKET_TRACE_CLEAN_UP_DOG] = LOAD_EBPF_FUNC_ADDR(ebpf_cleanup_dog);
  mFuncs[(int)ebpf_func::EBPF_SOCKET_TRACE_UPDATE_CONN_ROLE] = LOAD_EBPF_FUNC_ADDR(ebpf_update_conn_role);
  mFuncs[(int)ebpf_func::EBPF_SOCKET_TRACE_DISABLE_PROCESS] = LOAD_EBPF_FUNC_ADDR(ebpf_disable_process);
  mFuncs[(int)ebpf_func::EBPF_SOCKET_TRACE_UPDATE_CONN_ADDR] = LOAD_EBPF_FUNC_ADDR(ebpf_update_conn_addr);

  mOffsets[(int)ebpf_func::EBPF_INIT] = LOAD_UPROBE_OFFSET(mFuncs[(int)ebpf_func::EBPF_INIT]);
  mOffsets[(int)ebpf_func::EBPF_UPDATE] = LOAD_UPROBE_OFFSET(mFuncs[(int)ebpf_func::EBPF_UPDATE]);
  mOffsets[(int)ebpf_func::EBPF_SUSPEND] = LOAD_UPROBE_OFFSET(mFuncs[(int)ebpf_func::EBPF_SUSPEND]);
  mOffsets[(int)ebpf_func::EBPF_DEINIT] = LOAD_UPROBE_OFFSET(mFuncs[(int)ebpf_func::EBPF_DEINIT]);
  mOffsets[(int)ebpf_func::EBPF_REMOVE] = LOAD_UPROBE_OFFSET(mFuncs[(int)ebpf_func::EBPF_REMOVE]);
  mOffsets[(int)ebpf_func::EBPF_SOCKET_TRACE_CLEAN_UP_DOG] = LOAD_UPROBE_OFFSET(mFuncs[(int)ebpf_func::EBPF_SOCKET_TRACE_CLEAN_UP_DOG]);
  mOffsets[(int)ebpf_func::EBPF_SOCKET_TRACE_UPDATE_CONN_ROLE] = LOAD_UPROBE_OFFSET(mFuncs[(int)ebpf_func::EBPF_SOCKET_TRACE_UPDATE_CONN_ROLE]);
  mOffsets[(int)ebpf_func::EBPF_SOCKET_TRACE_DISABLE_PROCESS] = LOAD_UPROBE_OFFSET(mFuncs[(int)ebpf_func::EBPF_SOCKET_TRACE_DISABLE_PROCESS]);
  mOffsets[(int)ebpf_func::EBPF_SOCKET_TRACE_UPDATE_CONN_ADDR] = LOAD_UPROBE_OFFSET(mFuncs[(int)ebpf_func::EBPF_SOCKET_TRACE_UPDATE_CONN_ADDR]);

  // check function load success
  for (auto& x : mFuncs) {
    if (x == nullptr) return false;
  }

  // update meta
  mLib = std::move(tmp_lib);

  return true;
}

bool SourceManager::DynamicLibSuccess() {
#ifdef APSARA_UNIT_TEST_MAIN
    return true;
#endif
  if (!mLib) return false;
  for (auto x : mFuncs) {
    if (x == nullptr) return false;
  }
  return true;
}

void SourceManager::FillCommonConf(std::unique_ptr<nami::eBPFConfig>& conf) {
  conf->host_ip_ = mHostIp;
  conf->host_name_ = mHostName;
  conf->host_path_prefix_ = mHostPathPrefix;
  if (conf->plugin_type_ == nami::PluginType::NETWORK_OBSERVE) {
    auto cc = std::get<nami::NetworkObserveConfig>(conf->config_);
    // set so addr
    cc.so_ = std::filesystem::path(mBinaryPath) / mFullLibName;
    cc.so_size_ = cc.so_.length();
    cc.uprobe_offset_ = mOffsets[(int)ebpf_func::EBPF_SOCKET_TRACE_CLEAN_UP_DOG];
    cc.upcr_offset_ = mOffsets[(int)ebpf_func::EBPF_SOCKET_TRACE_UPDATE_CONN_ROLE];
    cc.upps_offset_ = mOffsets[(int)ebpf_func::EBPF_SOCKET_TRACE_DISABLE_PROCESS];
    cc.upca_offset_ = mOffsets[(int)ebpf_func::EBPF_SOCKET_TRACE_UPDATE_CONN_ADDR];
    conf->config_ = cc;
  }
}

bool SourceManager::CheckPluginRunning(nami::PluginType plugin_type) {
  if (!LoadDynamicLib(m_lib_name_)) {
    LOG_ERROR(sLogger, ("dynamic lib not load, plugin type:", int(plugin_type)));
    return false;
  }

  return mRunning[int(plugin_type)];
}

bool SourceManager::StartPlugin(nami::PluginType plugin_type, 
                std::variant<nami::NetworkObserveConfig, nami::ProcessConfig, nami::NetworkSecurityConfig, nami::FileSecurityConfig> config) {
  if (CheckPluginRunning(plugin_type)) {
    // plugin update ... 
    return UpdatePlugin(plugin_type, std::move(config));
  }

  // plugin not started ... 
  LOG_INFO(sLogger, ("begin to start plugin, type", int(plugin_type)));
  auto conf = std::make_unique<nami::eBPFConfig>();
  conf->plugin_type_ = plugin_type;
  conf->type = UpdataType::SECURE_UPDATE_TYPE_ENABLE_PROBE;
  conf->config_ = config;
  FillCommonConf(conf);
#ifdef APSARA_UNIT_TEST_MAIN
    mConfig = std::move(conf);
    mRunning[int(plugin_type)] = true;
    return true;
#endif
  void* f = mFuncs[(int)ebpf_func::EBPF_INIT];
  if (!f) {
    LOG_ERROR(sLogger, ("failed to load dynamic lib, init func ptr is null", int(plugin_type)));
    return false;
  }
  auto init_f = (init_func)f;
  int res = init_f(conf.release());
  if (!res) mRunning[int(plugin_type)] = true;
  return !res;
}

bool SourceManager::UpdatePlugin(nami::PluginType plugin_type, 
                std::variant<nami::NetworkObserveConfig, nami::ProcessConfig, nami::NetworkSecurityConfig, nami::FileSecurityConfig> config) {
  if (!CheckPluginRunning(plugin_type)) {
    LOG_ERROR(sLogger, ("plugin not started, type",  int(plugin_type)));
    return false;
  }

  auto conf = std::make_unique<nami::eBPFConfig>();
  conf->plugin_type_ = plugin_type;
  conf->type = UpdataType::SECURE_UPDATE_TYPE_CONFIG_CHAGE;
  conf->config_ = config;
  FillCommonConf(conf);
#ifdef APSARA_UNIT_TEST_MAIN
  mConfig = std::move(conf);
  return true;
#endif
  void* f = mFuncs[(int)ebpf_func::EBPF_UPDATE];
  if (!f) {
    LOG_ERROR(sLogger, ("failed to load dynamic lib, update func ptr is null", int(plugin_type)));
    return false;
  }

  auto update_f = (update_func)f;
  int res = update_f(conf.release());
  if (!res) mRunning[int(plugin_type)] = true;
  return !res;
}

bool SourceManager::StopAll() {
  if (!DynamicLibSuccess()) {
    LOG_WARNING(sLogger, ("dynamic lib not load, just exit", "need check"));
    return true;
  }

  for (size_t i = 0; i < mRunning.size(); i ++) {
    auto& x = mRunning[i];
    if (!x) continue;
    // stop plugin
    StopPlugin(static_cast<nami::PluginType>(i));
  }

#ifdef APSARA_UNIT_TEST_MAIN
  return true;
#endif

  // call deinit
  auto deinit_f = (deinit_func)mFuncs[(int)ebpf_func::EBPF_DEINIT];
  deinit_f();
  return true;
}

bool SourceManager::SuspendPlugin(nami::PluginType plugin_type) {
  if (!CheckPluginRunning(plugin_type)) {
    LOG_WARNING(sLogger, ("plugin not started, cannot suspend. type",  int(plugin_type)));
    return false;
  }
  auto config = std::make_unique<nami::eBPFConfig>();
  config->plugin_type_ = plugin_type;
  config->type = UpdataType::SECURE_UPDATE_TYPE_SUSPEND_PROBE;
#ifdef APSARA_UNIT_TEST_MAIN
  mConfig = std::move(config);
  return true;
#endif
  // ensure that sysak would not call handle()
  void* f = mFuncs[(int)ebpf_func::EBPF_SUSPEND];
  if (!f) {
    LOG_ERROR(sLogger, ("failed to load dynamic lib, suspend func ptr is null", int(plugin_type)));
    return false;
  }

  auto suspend_f = (suspend_func)f;
  int res = suspend_f(config.release());

  return !res;
}

bool SourceManager::StopPlugin(nami::PluginType plugin_type) {
  if (!CheckPluginRunning(plugin_type)) {
    LOG_WARNING(sLogger, ("plugin not started, do nothing. type",  int(plugin_type)));
    return true;
  }

  auto config = std::make_unique<nami::eBPFConfig>();
  config->plugin_type_ = plugin_type;
  config->type = UpdataType::SECURE_UPDATE_TYPE_DISABLE_PROBE;

#ifdef APSARA_UNIT_TEST_MAIN
  mConfig = std::move(config);
  mRunning[int(plugin_type)] = false;
  return true;
#endif

  void* f = mFuncs[(int)ebpf_func::EBPF_REMOVE];
  if (!f) {
    LOG_ERROR(sLogger, ("failed to load dynamic lib, remove func ptr is null", int(plugin_type)));
    return false;
  }

  auto remove_f = (remove_func)f;
  int res = remove_f(config.release());
  if (!res) mRunning[int(plugin_type)] = false;
  return !res;
}

}
}
