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

#include "Log.h"

namespace {
logtail::ebpf::eBPFLogHandler log_fn = nullptr;
} // namespace

void set_log_handler(logtail::ebpf::eBPFLogHandler fn) {
    if (log_fn == nullptr) {
        log_fn = fn;
    }
}

void ebpf_log(logtail::ebpf::eBPFLogType level, const char* format, ...) {
    if (log_fn == nullptr) {
        return;
    }

    va_list args;
    va_start(args, format);
    (void)log_fn(int16_t(level), format, args);
    va_end(args);
}

int libbpf_printf(enum libbpf_print_level level, const char* format, va_list args) {
    if (log_fn == nullptr) {
        return -1;
    }
    (void)log_fn(int16_t(level), format, args);
    return 0;
}
