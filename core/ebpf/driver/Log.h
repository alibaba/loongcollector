#pragma once

#include "ebpf/include/export.h"
#include <stdarg.h>

void set_log_handler(logtail::ebpf::eBPFLogHandler log_fn);
void ebpf_log(logtail::ebpf::eBPFLogType level,
		    const char *format, ...);