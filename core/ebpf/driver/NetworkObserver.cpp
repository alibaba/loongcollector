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

#include "ebpf/include/export.h"
#include "NetworkObserver.h"

int start_plugin(nami::eBPFConfig *arg) {
    // 1. load skeleton
    // 2. start consumer
    // 3. attach prog
    return 0;
}
int update_plugin(nami::eBPFConfig *arg) {
    // 1. suspend consumer
    // 2. detach prog
    // 3. set filter
    // 4. attach prog
    // 5. resume consumer
    return 0;
}

int stop_plugin(nami::PluginType) {
    // 1. detach prog
    // 2. stop consumer
    // 3. destruct skeleton
    return 0;
}

int suspend_plugin(nami::PluginType) {
    return 0;
}

int resume_plugin(nami::PluginType) {
    return 0;
}
