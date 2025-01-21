#pragma once

#include <stdarg.h>

#include "ebpf/include/export.h"

void set_log_handler(logtail::ebpf::eBPFLogHandler log_fn);
void ebpf_log(logtail::ebpf::eBPFLogType level, const char* format, ...);
