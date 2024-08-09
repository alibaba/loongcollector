/**
 * used for sysak
 */

#pragma once

using init_func = int (*)(void *);
using remove_func = int (*)(void *);
using suspend_func = int(*)(void *);
using deinit_func = void (*)(void);
using update_func = int(*)(void*);
