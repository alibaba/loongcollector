/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

const int PROCESSOR_INTERFACE_VERSION = 100;

struct processor_instance_t;

// 插件接口函数指针类型
typedef int (*processor_init_func_t)(struct processor_instance_t* /*ins*/, void* /*config*/, void* /*context*/);
typedef void (*processor_finialize_func_t)(void* /*plugin_state*/);
typedef void (*processor_process_func_t)(void* /*plugin_state*/, void* /*logGroup*/);

// 插件接口结构体
typedef struct processor_interface_t {
    int version; // 插件接口版本号
    const char* name; // 插件名称
    const char* language; // 插件语言
    processor_init_func_t init; // 插件初始化函数
    processor_finialize_func_t finalize; // 插件卸载函数
    processor_process_func_t process; // 插件测试函数
} processor_interface_t;

typedef struct processor_instance_t {
    const processor_interface_t* plugin;
    void* plugin_state; // 具体插件实例状态
} processor_instance_t;

#ifdef __cplusplus
}
#endif
