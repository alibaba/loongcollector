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

#include <coolbpf/bpf/libbpf.h>
#include <cstdarg>

#include "ebpf/include/export.h"

void set_log_handler(logtail::ebpf::eBPFLogHandler log_fn);
void ebpf_log(logtail::ebpf::eBPFLogType level, const char* format, ...);
int libbpf_printf(enum libbpf_print_level level, const char* format, va_list args);

#define EBPF_LOG(level, format, ...) \
    do { \
        ebpf_log(level, "%s:%d\t" format, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)
