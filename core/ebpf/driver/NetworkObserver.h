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

#include "ebpf/include/export.h"
#include "linux/bpf.h"

extern "C" {

typedef void (*perf_process_event_fn)(void *ctx, int cpu,
				      void *data, uint32_t size);
typedef void (*perf_loss_event_fn)(void *ctx, int cpu, unsigned long long cnt);

}

using start_plugin_func = int (*)(nami::PluginConfig *);
using update_plugin_func = int (*)(nami::PluginConfig *);
using stop_plugin_func = void (*)(nami::PluginType);
using suspend_plugin_func = int (*)(nami::PluginType);
using resume_plugin_func = int (*)(nami::PluginType);


/// op funcs ////
using prepare_skeleton_func = int (*)(nami::PluginType);
using destroy_skeleton_func = int (*)(nami::PluginType);
using dynamic_attach_bpf_prog_func = int (*)(nami::PluginType, const char*);
using dynamic_detach_bpf_prog_func = int (*)(nami::PluginType, const char*);
using set_tailcall_func = int (*)(nami::PluginType, const char*, const char**, size_t);

using search_map_fd_func = int (*)(nami::PluginType, const char*);
using create_bpf_map_func = int(*)(nami::PluginType, enum bpf_map_type, int, int, int, unsigned int);
using get_bpf_map_fd_by_id = int(*)(nami::PluginType, int);

// using delete_inner_map_func = int (*)(nami::PluginType, ebpf_map_name, const char*, void*);
// using delete_inner_map_elem_func = int (*)(nami::PluginType, ebpf_map_name, const char*, void*, void*);
// using lookup_inner_map_elem_func = int (*)(nami::PluginType, ebpf_map_name, const char*, void*, void*, void*);
// using update_inner_map_elem_func = int (*)(nami::PluginType, ebpf_map_name, const char*, void*, void*, void*, uint64_t);

using lookup_bpf_map_elem_func = int (*)(nami::PluginType, const char*, void*, void*);
using remove_bpf_map_elem_func = int (*)(nami::PluginType, const char*, void*);
using update_bpf_map_elem_func = int (*)(nami::PluginType, const char*, void*, void*, uint64_t);

using create_perf_buffer_func = void* (*)(nami::PluginType, const char*, int, void*, perf_process_event_fn, perf_loss_event_fn);
using delete_perf_buffer_func = void (*)(nami::PluginType, void*);
using poll_perf_buffer_func = int (*)(nami::PluginType, void*, int32_t, int32_t*, int);

using init_network_observer_func = int (*)(char*, int32_t, char*, int32_t, long, long, long, long);
using start_network_observer_func = int (*)(void);
using stop_network_observer_func = int (*)(void);
using network_observer_poll_events_func = int32_t (*)(int32_t, int32_t*);
using network_observer_config_func = void (*)(int32_t, int32_t, int32_t, void**, int32_t*);

using network_observer_setup_net_data_process_func_func = void (*)(net_data_process_func_t, void*);
using network_observer_setup_net_event_process_func_func = void (*)(net_ctrl_process_func_t, void*);
using network_observer_setup_net_statistics_process_func_func = void (*)(net_statistics_process_func_t, void*);
using network_observer_setup_net_lost_func_func = void (*)(net_lost_func_t, void*);
using network_observer_setup_print_func_func = void (*)(net_print_fn_t);

extern "C" {
int start_plugin(nami::PluginConfig *arg);
int update_plugin(nami::PluginConfig *arg);
int stop_plugin(nami::PluginType);
int suspend_plugin(nami::PluginType);
int resume_plugin(nami::PluginType);


/**
 * FOR GENERIC
 */
int prepare_skeleton(nami::PluginType type);
int destroy_skeleton(nami::PluginType type);
int dynamic_attach_bpf_prog(nami::PluginType type, const char* prog_name);
int dynamic_detach_bpf_prog(nami::PluginType type, const char* prog_name);
int set_tailcall(nami::PluginType type, const char* map_name, const char** functions, size_t function_count);

// int delete_inner_map(nami::PluginType type, ebpf_map_name map, const char* outter_map_name, void* outter_key);
// int delete_inner_map_elem(nami::PluginType type, ebpf_map_name map, const char* outter_map_name, void* outter_key, void* inner_key);
// int lookup_inner_map_elem(nami::PluginType type, ebpf_map_name map, const char* outter_map_name, void* outter_key, void* inner_key, void* inner_val);
// int update_inner_map_elem(nami::PluginType type, ebpf_map_name map, const char* outter_map_name, void* outter_key, void* inner_key, void* inner_value, uint64_t flag);

// map ops
int search_map_fd(nami::PluginType type, const char* map_name);
int create_bpf_map(nami::PluginType type, enum bpf_map_type map_type, int key_size, int value_size, int max_entries, unsigned int map_flags);
int get_bpf_map_fd_by_id(nami::PluginType type, int id);

int lookup_bpf_map_elem(nami::PluginType type, const char* map_name, void* key, void* value);
int remove_bpf_map_elem(nami::PluginType type, const char* map_name, void* key);
int update_bpf_map_elem(nami::PluginType type, const char* map_name, void* key, void* value, uint64_t flag);

void* create_perf_buffer(nami::PluginType type, const char* map_name, int page_cnt, void* ctx, perf_process_event_fn data_cb, perf_loss_event_fn loss_cb);
void delete_perf_buffer(nami::PluginType type, void* pb);
int poll_perf_buffer(nami::PluginType type, void* pb, int32_t max_events, int32_t *stop_flag, int time_out_ms);

/**
 * FOR NETWORK OBSERVER
 */
// control plane
int init_network_observer(char *btf, int32_t btf_size, char *so, int32_t so_size, long uprobe_offset,
                  long upca_offset, long upps_offset, long upcr_offset);
int start_network_observer(void);
int stop_network_observer(void);
int32_t network_observer_poll_events(int32_t max_events, int32_t *stop_flag);
void network_observer_config(int32_t opt1, int32_t opt2, int32_t params_count, void **params, int32_t *params_len);

// data plane
void network_observer_setup_net_data_process_func(net_data_process_func_t func, void *custom_data);
void network_observer_setup_net_event_process_func(net_ctrl_process_func_t func, void *custom_data);
void network_observer_setup_net_statistics_process_func(net_statistics_process_func_t func, void *custom_data);
void network_observer_setup_net_lost_func(net_lost_func_t func, void *custom_data);
void network_observer_setup_print_func(net_print_fn_t func);

}
