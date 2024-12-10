/**
 * used for sysak
 */

#pragma once

#include "ebpf/include/export.h"

using init_func = int (*)(nami::eBPFConfig*);
using remove_func = int (*)(nami::eBPFConfig*);
using deinit_func = void (*)(void);
using suspend_func = int (*)(nami::eBPFConfig*);
using update_func = int (*)(nami::eBPFConfig*);