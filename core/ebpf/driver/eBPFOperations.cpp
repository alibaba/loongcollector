#include "eBPFOperations.h"
#include "eBPFWrapper.h"
#include "ebpf/include/export.h"
#include "NetworkObserver.h"
#include "Log.h"

#include <coolbpf/security.skel.h>
#include <net.h>
/////////////////////////////////////

/**
 * FOR GENERIC
 */

std::shared_ptr<logtail::ebpf::BPFWrapper<security_bpf>> security_bpf_wrapper = nullptr;
int prepare_skeleton(logtail::ebpf::PluginType type) {
  if (!security_bpf_wrapper) {
    security_bpf_wrapper = logtail::ebpf::BPFWrapper<security_bpf>::Create();
  }
  return security_bpf_wrapper->Init();
}

int destroy_skeleton(logtail::ebpf::PluginType type) {
  if (security_bpf_wrapper) {
    security_bpf_wrapper->Destroy();
  }
  return 0;
}

int dynamic_attach_bpf_prog(logtail::ebpf::PluginType type, const char* prog_name) {
  return security_bpf_wrapper->DynamicAttachBPFObject({{std::string(prog_name), true}});
}
int dynamic_detach_bpf_prog(logtail::ebpf::PluginType type, const char* prog_name) {
  return security_bpf_wrapper->DynamicDetachBPFObject({{std::string(prog_name), true}});
}
int set_tailcall(logtail::ebpf::PluginType type, const char* map_name, const char** functions, size_t function_count) {
  std::vector<std::string> funcs;
  for (size_t i = 0; i < function_count; ++i) {
      const char* function = functions[i];
      funcs.push_back(std::string(function));
  }
  return security_bpf_wrapper->SetTailCall(std::string(map_name), funcs);
}

// int delete_inner_map(logtail::ebpf::PluginType type, ebpf_map_name map, const char* outter_map_name, void* outter_key) {
//   return security_bpf_wrapper->DeleteInnerMap<>(std::string(outter_map_name), outter_key);
// }
// int delete_inner_map_elem(logtail::ebpf::PluginType type, ebpf_map_name map, const char* outter_map_name, void* outter_key, void* inner_key) {
//   return security_bpf_wrapper->DeleteInnerMapElem<decltype(maps[int(map)])>(std::string(outter_map_name), outter_key, inner_key);
// }
// int lookup_inner_map_elem(logtail::ebpf::PluginType type, ebpf_map_name map, const char* outter_map_name, void* outter_key, void* inner_key, void* inner_val) {
//   return security_bpf_wrapper->LookupInnerMapElem<decltype(maps[int(map)])>(std::string(outter_map_name), outter_key, inner_key, inner_val);
// }
// int update_inner_map_elem(logtail::ebpf::PluginType type, ebpf_map_name map, const char* outter_map_name, void* outter_key, void* inner_key, void* inner_value, uint64_t flag) {
//   return security_bpf_wrapper->UpdateInnerMapElem<decltype(maps[int(map)])>(std::string(outter_map_name), outter_key, inner_key, inner_value, flag);
// }

int search_map_fd(logtail::ebpf::PluginType type, const char* map_name) {
  return security_bpf_wrapper->SearchMapFd(std::string(map_name));
}

int create_bpf_map(logtail::ebpf::PluginType type, enum bpf_map_type map_type, int key_size, int value_size, int max_entries, unsigned int map_flags) {
  return security_bpf_wrapper->CreateBPFMap(map_type, key_size, value_size, max_entries, map_flags);
}

int get_bpf_map_fd_by_id(logtail::ebpf::PluginType type, int id) {
  return security_bpf_wrapper->GetBPFMapFdById(id);
}

int lookup_bpf_map_elem(logtail::ebpf::PluginType type, const char* map_name, void* key, void* value) {
  return security_bpf_wrapper->LookupBPFHashMap(std::string(map_name), key, value);
}

int remove_bpf_map_elem(logtail::ebpf::PluginType type, const char* map_name, void* key) {
  return security_bpf_wrapper->RemoveBPFHashMap(std::string(map_name), key);
}

int update_bpf_map_elem(logtail::ebpf::PluginType type, const char* map_name, void* key, void* value, uint64_t flag) {
  return security_bpf_wrapper->UpdateBPFHashMap(std::string(map_name), key, value, flag);
}

void* create_perf_buffer(logtail::ebpf::PluginType type, const char* map_name, int page_cnt, void* ctx, perf_process_event_fn data_cb, perf_loss_event_fn loss_cb) {
  return security_bpf_wrapper->CreatePerfBuffer(std::string(map_name), page_cnt, ctx, data_cb, loss_cb);
}
void delete_perf_buffer(logtail::ebpf::PluginType type, void* pb) {
  security_bpf_wrapper->DeletePerfBuffer(pb);

}
int poll_perf_buffer(logtail::ebpf::PluginType type, void* pb, int32_t max_events, int time_out_ms) {
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
  return ebpf_poll_events(max_events, stop_flag, 0);
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
