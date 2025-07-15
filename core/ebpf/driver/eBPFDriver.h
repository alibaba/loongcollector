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

#pragma once

#include "ebpf/include/export.h"

using set_logger_func = int (*)(logtail::ebpf::eBPFLogHandler fn);
using start_plugin_func = int (*)(logtail::ebpf::PluginConfig*);
using update_plugin_func = int (*)(logtail::ebpf::PluginConfig*);
using stop_plugin_func = int (*)(logtail::ebpf::PluginType);
using suspend_plugin_func = int (*)(logtail::ebpf::PluginType);
using resume_plugin_func = int (*)(logtail::ebpf::PluginConfig*);
using poll_plugin_pbs_func = int (*)(logtail::ebpf::PluginType, int32_t, int32_t*, int);
using set_networkobserver_config_func = void (*)(int32_t, int32_t);
using set_networkobserver_cid_filter_func = void (*)(const char*, size_t, bool);
using update_bpf_map_elem_func = int (*)(logtail::ebpf::PluginType, const char*, void*, void*, uint64_t);

extern "C" {
int set_logger(logtail::ebpf::eBPFLogHandler fn);

// control plane
int start_plugin(logtail::ebpf::PluginConfig* arg);
int update_plugin(logtail::ebpf::PluginConfig* arg);
int stop_plugin(logtail::ebpf::PluginType);

int suspend_plugin(logtail::ebpf::PluginType);
int resume_plugin(logtail::ebpf::PluginConfig* arg);

// data plane
int poll_plugin_pbs(logtail::ebpf::PluginType type, int32_t max_events, int32_t* stop_flag, int timeout_ms);

// networkobserver 特有，后续采集配置改造后会
void set_networkobserver_config(int32_t opt, int32_t value);
void set_networkobserver_cid_filter(const char* container_id, size_t length, bool update);

// oprations
int update_bpf_map_elem(logtail::ebpf::PluginType type, const char* map_name, void* key, void* value, uint64_t flag);
}
