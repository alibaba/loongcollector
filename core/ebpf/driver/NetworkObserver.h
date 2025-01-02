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

using init_func = int (*)(nami::eBPFConfig *);
using remove_func = int (*)(nami::eBPFConfig *);
using deinit_func = void (*)(void);
using suspend_func = int (*)(nami::eBPFConfig *);
using update_func = int (*)(nami::eBPFConfig *);

extern "C" {
int start_plugin(nami::eBPFConfig *arg);
int update_plugin(nami::eBPFConfig *arg);
int stop_plugin(nami::PluginType);
int suspend_plugin(nami::PluginType);
int resume_plugin(nami::PluginType);
}
