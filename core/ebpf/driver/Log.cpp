#include "Log.h"

logtail::ebpf::eBPFLogHandler log_fn = nullptr;

void set_log_handler(logtail::ebpf::eBPFLogHandler fn) {
    if (!log_fn) log_fn = fn;
}

void ebpf_log(logtail::ebpf::eBPFLogType level,
		    const char *format, ...) {
	va_list args;

	va_start(args, format);
    if (log_fn) {
        (void)log_fn(int16_t(level), format, args);
    }
	
	va_end(args);
}