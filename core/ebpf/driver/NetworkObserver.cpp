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


#include <iostream>
#include "ebpf/include/export.h"
#include <coolbpf/security.skel.h>
extern "C" {
#include "NetworkObserver.h"
#include <coolbpf/net.h>
#include <coolbpf/security/data_msg.h>
#include <coolbpf/security/msg_type.h>
#include <coolbpf/security/bpf_process_event_type.h>
#include <sys/resource.h>
}

#include "eBPFWrapper.h"
#include "Log.h"

int set_logger(logtail::ebpf::eBPFLogHandler fn) {
  set_log_handler(fn);
  return 0;
}

void bump_memlock_rlimit(void){
  struct rlimit rlim_new = {
    .rlim_cur = RLIM_INFINITY,
    .rlim_max = RLIM_INFINITY,
  };

  if (setrlimit(RLIMIT_MEMLOCK, &rlim_new)) {
    fprintf(stderr, "Failed to increase RLIMIT_MEMLOCK limit!\n");
    exit(1);
  }
}

std::array<std::vector<void*>, size_t(logtail::ebpf::PluginType::MAX)> gPluginPbs;

std::shared_ptr<logtail::ebpf::BPFWrapper<security_bpf>> wrapper = logtail::ebpf::BPFWrapper<security_bpf>::Create();
std::atomic<bool> gflag;
std::vector<std::thread> pws;
void SetCoolBpfConfig(int32_t opt, int32_t value) {
    int32_t *params[] = {&value};
    int32_t paramsLen[] = {4};
    ebpf_config(opt, 0, 1, (void **)params, paramsLen);
}

int start_plugin(logtail::ebpf::PluginConfig *arg) {
    // 1. load skeleton
    // 2. start consumer
    // 3. attach prog
    ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG, "enter start_plugin, arg is null: %d \n", arg == nullptr);
    bump_memlock_rlimit();

    switch (arg->mPluginType)
    {
    case logtail::ebpf::PluginType::NETWORK_OBSERVE:{
        auto config = std::get_if<logtail::ebpf::NetworkObserveConfig>(&arg->mConfig);
        ebpf_setup_print_func(config->mLogHandler);
        ebpf_setup_net_event_process_func(config->mCtrlHandler, config->mCustomCtx);
        ebpf_setup_net_data_process_func(config->mDataHandler, config->mCustomCtx);
        ebpf_setup_net_statistics_process_func(config->mStatsHandler, config->mCustomCtx);
        ebpf_setup_net_lost_func(config->mLostHandler, config->mCustomCtx);
        
        int err = ebpf_init(nullptr, 0, config->mSo.data(), static_cast<int32_t>(config->mSo.length()),
                      config->mUprobeOffset, config->mUpcaOffset, config->mUppsOffset, config->mUpcrOffset);
        // config
        SetCoolBpfConfig((int32_t)PROTOCOL_FILTER, 1);
        SetCoolBpfConfig((int32_t)TGID_FILTER, -1);
        SetCoolBpfConfig((int32_t)PORT_FILTER, -1);
        SetCoolBpfConfig((int32_t)SELF_FILTER, getpid());
        SetCoolBpfConfig((int32_t)DATA_SAMPLING, 100);

        // TODO
        // if (config->mEnableCidFilter) {
        //   SetCoolBpfConfig((int32_t)CONTAINER_ID_FILTER, offset);
        // }
        // 
        err = ebpf_start();
        break;
    }
    case logtail::ebpf::PluginType::PROCESS_SECURITY:{
        int err = wrapper->Init();
        auto config = std::get_if<logtail::ebpf::ProcessConfig>(&arg->mConfig);
        std::vector<logtail::ebpf::AttachProgOps> attach_ops = {logtail::ebpf::AttachProgOps("event_exit_acct_process", true),
            logtail::ebpf::AttachProgOps("event_wake_up_new_task", true),
            logtail::ebpf::AttachProgOps("event_exit_disassociate_ctty", true),
            logtail::ebpf::AttachProgOps("event_execve", true),
            logtail::ebpf::AttachProgOps("execve_rate", false),
            logtail::ebpf::AttachProgOps("execve_send", false),
            logtail::ebpf::AttachProgOps("filter_prog", false),
        };

        std::vector<std::pair<const std::string, const std::vector<std::string>>> tail_calls = {
            {"execve_calls", {"execve_rate", "execve_send"}}};
        
        // set tail call
        for (auto& tail_call : tail_calls) {
            auto ret = wrapper->SetTailCall(tail_call.first, tail_call.second);
            if (ret != 0) {
                std::cout << "[ResourceInstance] SetTailCall fail :" << ret << std::endl;
            }
        }

        std::vector<logtail::ebpf::PerfBufferOps> perf_buffers;
        for (auto& spec : config->mPerfBufferSpec) {
          void* pb = wrapper->CreatePerfBuffer(spec.mName, spec.mSize, spec.mCtx,
            static_cast<perf_buffer_sample_fn>(spec.mSampleHandler), 
            static_cast<perf_buffer_lost_fn>(spec.mLostHandler));
          gPluginPbs[int(arg->mPluginType)].push_back(pb);
        }
        // TODO use handler from config ... 
        // std::vector<logtail::ebpf::PerfBufferOps> perf_buffers = {
        //     logtail::ebpf::PerfBufferOps("tcpmon_map", 50 * 1024 * 1024, HandleKernelProcessEvent,
        //                     HandleKernelProcessEventLost),
        // };
        // poll perf buffers ...

        // set perf buffer 
        gflag = true;
        // pws = wrapper->AttachPerfBuffers(nullptr, perf_buffers, std::ref(gflag));
        // std::cout << "[ResourceInstance] begin to dynamic attach bpf object" << std::endl;
        int ret = wrapper->DynamicAttachBPFObject(attach_ops);
        if (ret != 0) {
            std::cout << "[ResourceInstance] DynamicAttachBPFObject fail :" << ret << std::endl;
        }
        std::cout << "[ResourceInstance] dynamic attach bpf object, ret:" << ret << std::endl;
        break;
    }
    default:
        break;
    }
    return 0;
}

int poll_plugin_pbs(logtail::ebpf::PluginType type, int32_t max_events, int32_t *stop_flag, int timeout_ms) {
  if (type == logtail::ebpf::PluginType::NETWORK_OBSERVE) {
    return ebpf_poll_events(max_events, stop_flag, timeout_ms);
  }
  
  // find pbs
  auto& pbs = gPluginPbs[int(type)];
  if (pbs.empty()) {
    ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN, "no pbs registered for type:%d \n", type);
    return -1;
  }
  int cnt = 0;
  for (auto& x : pbs) {
    int ret = wrapper->PollPerfBuffer(x, max_events, timeout_ms);
    if (ret < 0 && errno != EINTR) {
      ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN, "poll perf buffer failed ... ");
    } else {
      cnt += ret;
    }
  }
  return cnt;
}

int update_plugin(logtail::ebpf::PluginConfig *arg) {
    // 1. suspend consumer
    // 2. detach prog
    // 3. set filter
    // 4. attach prog
    // 5. resume consumer
    return 0;
}

int stop_plugin(logtail::ebpf::PluginType pluginType) {
    // 1. detach prog
    // 2. stop consumer
    // 3. destruct skeleton
    switch (pluginType)
    {
    case logtail::ebpf::PluginType::NETWORK_OBSERVE:
        return ebpf_stop();
    case logtail::ebpf::PluginType::PROCESS_SECURITY:

        break;
    default:
        break;
    }
    return 0;
}

int suspend_plugin(logtail::ebpf::PluginType) {
    return 0;
}

int resume_plugin(logtail::ebpf::PluginType) {
    return 0;
}


