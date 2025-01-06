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



/////////////////////////////////////

/**
 * FOR GENERIC
 */

std::shared_ptr<nami::BPFWrapper<security_bpf>> security_bpf_wrapper = nullptr;
int prepare_skeleton(nami::PluginType type) {
  if (!security_bpf_wrapper) {
    security_bpf_wrapper = nami::BPFWrapper<security_bpf>::Create();
  }
  return security_bpf_wrapper->Init();
}

int destroy_skeleton(nami::PluginType type) {
  if (security_bpf_wrapper) {
    security_bpf_wrapper->Destroy();
  }
  return 0;
}

int dynamic_attach_bpf_prog(nami::PluginType type, const char* prog_name) {
  return security_bpf_wrapper->DynamicAttachBPFObject({{std::string(prog_name), true}});
}
int dynamic_detach_bpf_prog(nami::PluginType type, const char* prog_name) {
  return security_bpf_wrapper->DynamicDetachBPFObject({{std::string(prog_name), true}});
}
int set_tailcall(nami::PluginType type, const char* map_name, const char** functions, size_t function_count) {
  std::vector<std::string> funcs;
  for (size_t i = 0; i < function_count; ++i) {
      const char* function = functions[i];
      funcs.push_back(std::string(function));
  }
  return security_bpf_wrapper->SetTailCall(std::string(map_name), funcs);
}

// int delete_inner_map(nami::PluginType type, ebpf_map_name map, const char* outter_map_name, void* outter_key) {
//   return security_bpf_wrapper->DeleteInnerMap<>(std::string(outter_map_name), outter_key);
// }
// int delete_inner_map_elem(nami::PluginType type, ebpf_map_name map, const char* outter_map_name, void* outter_key, void* inner_key) {
//   return security_bpf_wrapper->DeleteInnerMapElem<decltype(maps[int(map)])>(std::string(outter_map_name), outter_key, inner_key);
// }
// int lookup_inner_map_elem(nami::PluginType type, ebpf_map_name map, const char* outter_map_name, void* outter_key, void* inner_key, void* inner_val) {
//   return security_bpf_wrapper->LookupInnerMapElem<decltype(maps[int(map)])>(std::string(outter_map_name), outter_key, inner_key, inner_val);
// }
// int update_inner_map_elem(nami::PluginType type, ebpf_map_name map, const char* outter_map_name, void* outter_key, void* inner_key, void* inner_value, uint64_t flag) {
//   return security_bpf_wrapper->UpdateInnerMapElem<decltype(maps[int(map)])>(std::string(outter_map_name), outter_key, inner_key, inner_value, flag);
// }

int search_map_fd(nami::PluginType type, const char* map_name) {
  return security_bpf_wrapper->SearchMapFd(std::string(map_name));
}

int create_bpf_map(nami::PluginType type, enum bpf_map_type map_type, int key_size, int value_size, int max_entries, unsigned int map_flags) {
  return security_bpf_wrapper->CreateBPFMap(map_type, key_size, value_size, max_entries, map_flags);
}

int get_bpf_map_fd_by_id(nami::PluginType type, int id) {
  return security_bpf_wrapper->GetBPFMapFdById(id);
}

int lookup_bpf_map_elem(nami::PluginType type, const char* map_name, void* key, void* value) {
  return security_bpf_wrapper->LookupBPFHashMap(std::string(map_name), key, value);
}

int remove_bpf_map_elem(nami::PluginType type, const char* map_name, void* key) {
  return security_bpf_wrapper->RemoveBPFHashMap(std::string(map_name), key);
}

int update_bpf_map_elem(nami::PluginType type, const char* map_name, void* key, void* value, uint64_t flag) {
  return security_bpf_wrapper->UpdateBPFHashMap(std::string(map_name), key, value, flag);
}

void* create_perf_buffer(nami::PluginType type, const char* map_name, int page_cnt, void* ctx, perf_process_event_fn data_cb, perf_loss_event_fn loss_cb) {
  return security_bpf_wrapper->CreatePerfBuffer(std::string(map_name), page_cnt, ctx, data_cb, loss_cb);
}
void delete_perf_buffer(nami::PluginType type, void* pb) {
  security_bpf_wrapper->DeletePerfBuffer(pb);

}
int poll_perf_buffer(nami::PluginType type, void* pb, int32_t max_events, int time_out_ms) {
  return security_bpf_wrapper->PollPerfBuffer(pb, max_events, time_out_ms);
}

/**
 * FOR NETWORK OBSERVER
 */
// control plane
int init_network_observer(char *btf, int32_t btf_size, char *so, int32_t so_size, long uprobe_offset,
                  long upca_offset, long upps_offset, long upcr_offset) {
  return ebpf_init(btf, btf_size, so, so_size, uprobe_offset, upca_offset, upps_offset, upcr_offset);
}
int start_network_observer(void) {
  return ebpf_start();
}
int stop_network_observer(void) {
  return ebpf_stop();
}

int32_t network_observer_poll_events(int32_t max_events, int32_t *stop_flag) {
  return ebpf_poll_events(max_events, stop_flag);
}
void network_observer_config(int32_t opt1, int32_t opt2, int32_t params_count, void **params, int32_t *params_len) {
  ebpf_config(opt1, opt2, params_count, params, params_len);
}

// data plane
void network_observer_setup_net_data_process_func(net_data_process_func_t func, void *custom_data) {
  return ebpf_setup_net_data_process_func(func, custom_data);
}
void network_observer_setup_net_event_process_func(net_ctrl_process_func_t func, void *custom_data) {
  return ebpf_setup_net_event_process_func(func, custom_data);
}
void network_observer_setup_net_statistics_process_func(net_statistics_process_func_t func, void *custom_data) {
  return ebpf_setup_net_statistics_process_func(func, custom_data);
}
void network_observer_setup_net_lost_func(net_lost_func_t func, void *custom_data) {
  return ebpf_setup_net_lost_func(func, custom_data);
}
void network_observer_setup_print_func(net_print_fn_t func) {
  return ebpf_setup_print_func(func);
}

