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

std::shared_ptr<nami::BPFWrapper<security_bpf>> wrapper = nami::BPFWrapper<security_bpf>::Create();
std::atomic<bool> gflag;
std::vector<std::thread> pws;
void SetCoolBpfConfig(int32_t opt, int32_t value) {
    int32_t *params[] = {&value};
    int32_t paramsLen[] = {4};
    ebpf_config(opt, 0, 1, (void **)params, paramsLen);
}

// for process handler ...
void HandleKernelProcessEventLost(void *ctx, int cpu, __u64 lost_cnt) {
  std::cout << "lost " << lost_cnt << " events on CPU:" << cpu << std::endl;
}

void HandleKernelProcessEvent(void *ctx, int cpu, void *data, __u32 data_sz)
{
//   BaseManager* ss = static_cast<BaseManager*>(ctx);
 std::cout << "[handle_event] enter ..." ;
  if (!data) {
    std::cout << "[handle_event] data is null!" ;
    return;
  }
  auto common = static_cast<struct msg_common*>(data);
  switch (common->op) {
    case MSG_OP_CLONE: {
      auto event = static_cast<struct msg_clone_event*>(data);
     std::cout << "======== [DUMP] msg_clone_event: ===========" << std::endl;
    //  print_msg_clone_event(*event);
     std::cout << "============================================" << std::endl;
    //   ss->RecordCloneEvent(event);
      break;
    }
    case MSG_OP_EXIT: {
      auto event = static_cast<struct msg_exit*>(data);
    //   ss->RecordExitEvent(event);

     std::cout << "========= [DUMP] msg_exit_event: ===========" << std::endl;
    //  print_msg_exit_event(*event);
     std::cout << "============================================" << std::endl;
      break;
    }
    case MSG_OP_EXECVE: {
      auto event_ptr = static_cast<struct msg_execve_event*>(data);
     std::cout << "======== [DUMP] msg_execve_event: ==========" ;
    //  print_msg_execve_event(*event_ptr);
     std::cout << "============================================" ;
    //   ss->RecordExecveEvent(event_ptr);
      break;
    }
    case MSG_OP_DATA: {
      auto event_ptr = static_cast<msg_data*>(data);
      std::cout << "======== [DUMP] msg_data: ==========" << std::endl;
      std::cout << "  pid:" << event_ptr->id.pid << " time:" << event_ptr->id.time << std::endl;
      std::cout << "  data:" << event_ptr->arg << std::endl;
      std::cout << "============================================" << std::endl;
      break;
    }
    case MSG_OP_THROTTLE: {
      auto event_ptr = static_cast<msg_throttle *>(data);
     std::cout << "======== [DUMP] msg_throttle: ==========" << std::endl;
     std::cout << "============================================" << std::endl;
      break;
    }
    default: {
      std::cout << "Unknown event op: " << static_cast<int>(common->op) ;
      break;
    }
  }
}


int start_plugin(nami::PluginConfig *arg) {
    // 1. load skeleton
    // 2. start consumer
    // 3. attach prog

    std::cout << "enter start_plugin, arg is null:" << (arg == nullptr) << std::endl;
    bump_memlock_rlimit();

    switch (arg->mPluginType)
    {
    case nami::PluginType::NETWORK_OBSERVE:{
        auto config = std::get_if<nami::NetworkObserveConfig>(&arg->mConfig);
        ebpf_setup_print_func(config->mPrintHandler);
        ebpf_setup_net_event_process_func(config->mCtrlHandler, config->mCustomCtx);
        ebpf_setup_net_data_process_func(config->mDataHandler, config->mCustomCtx);
        ebpf_setup_net_statistics_process_func(config->mStatsHandler, config->mCustomCtx);
        ebpf_setup_net_lost_func(config->mLostHandler, config->mCustomCtx);
        
        int err = ebpf_init(nullptr, 0, config->so_.data(), static_cast<int32_t>(config->so_.length()),
                      config->uprobe_offset_, config->upca_offset_, config->upps_offset_, config->upcr_offset_);
        // config
        SetCoolBpfConfig((int32_t)PROTOCOL_FILTER, 1);
        SetCoolBpfConfig((int32_t)TGID_FILTER, -1);
        SetCoolBpfConfig((int32_t)PORT_FILTER, -1);
        SetCoolBpfConfig((int32_t)SELF_FILTER, getpid());
        SetCoolBpfConfig((int32_t)DATA_SAMPLING, 100);

        // TODO
        // SetCoolBpfConfig((int32_t)CONTAINER_ID_FILTER, offset);
        err = ebpf_start();
        // poll perf buffer
        // ebpf_poll_events();
        break;
    }
    case nami::PluginType::PROCESS_SECURITY:{
        int err = wrapper->Init();
        std::vector<nami::AttachProgOps> attach_ops = {nami::AttachProgOps("event_exit_acct_process", true),
            nami::AttachProgOps("event_wake_up_new_task", true),
            nami::AttachProgOps("event_exit_disassociate_ctty", true),
            nami::AttachProgOps("event_execve", true),
            nami::AttachProgOps("execve_rate", false),
            nami::AttachProgOps("execve_send", false),
            nami::AttachProgOps("filter_prog", false),
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

        std::vector<nami::PerfBufferOps> perf_buffers = {
            nami::PerfBufferOps("tcpmon_map", 50 * 1024 * 1024, HandleKernelProcessEvent,
                            HandleKernelProcessEventLost),
        };

        // set perf buffer 
        gflag = true;
        pws = wrapper->AttachPerfBuffers(nullptr, perf_buffers, std::ref(gflag));

        std::cout << "[ResourceInstance] begin to dynamic attach bpf object" << std::endl;
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

int update_plugin(nami::PluginConfig *arg) {
    // 1. suspend consumer
    // 2. detach prog
    // 3. set filter
    // 4. attach prog
    // 5. resume consumer
    return 0;
}

int stop_plugin(nami::PluginType pluginType) {
    // 1. detach prog
    // 2. stop consumer
    // 3. destruct skeleton
    switch (pluginType)
    {
    case nami::PluginType::NETWORK_OBSERVE:
        return ebpf_stop();
    case nami::PluginType::PROCESS_SECURITY:

        break;
    default:
        break;
    }
    return 0;
}

int suspend_plugin(nami::PluginType) {
    return 0;
}

int resume_plugin(nami::PluginType) {
    return 0;
}
