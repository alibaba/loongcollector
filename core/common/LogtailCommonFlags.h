/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include "Flags.h"

//////////////////////////////////////////////////////////////////////////
// TODO: This header file only contain flags that exported to users.
// So, related source file only contain corresponding definitions.
//
// For flags that used internally, declare and define in their own header
// and source files.
/////////////////////////////////////////////////////////////////////////

DECLARE_FLAG_INT32(default_max_inotify_watch_num);
DECLARE_FLAG_INT32(default_tail_limit_kb);
DECLARE_FLAG_STRING(logtail_send_address);
DECLARE_FLAG_STRING(logtail_config_address);
DECLARE_FLAG_INT32(cpu_limit_num);
DECLARE_FLAG_INT32(mem_limit_num);
DECLARE_FLAG_DOUBLE(cpu_usage_up_limit);
DECLARE_FLAG_DOUBLE(pub_cpu_usage_up_limit);
DECLARE_FLAG_INT64(memory_usage_up_limit);
DECLARE_FLAG_INT64(pub_memory_usage_up_limit);
DECLARE_FLAG_INT32(monitor_interval);
DECLARE_FLAG_INT32(batch_send_interval);
DECLARE_FLAG_INT32(batch_send_metric_size);
DECLARE_FLAG_INT32(max_holded_data_size);
DECLARE_FLAG_INT32(pub_max_holded_data_size);
DECLARE_FLAG_INT32(log_expire_time);
DECLARE_FLAG_BOOL(default_secondary_storage);
DECLARE_FLAG_STRING(logtail_profile_snapshot);
DECLARE_FLAG_STRING(logtail_line_count_snapshot);
DECLARE_FLAG_STRING(logtail_integrity_snapshot);
DECLARE_FLAG_INT32(profile_data_send_interval);
DECLARE_FLAG_STRING(local_machine_uuid);
DECLARE_FLAG_STRING(user_defined_id_file);
DECLARE_FLAG_STRING(logtail_sys_conf_users_dir);
DECLARE_FLAG_STRING(logtail_profile_aliuid);
DECLARE_FLAG_STRING(logtail_profile_access_key_id);
DECLARE_FLAG_STRING(logtail_profile_access_key);
DECLARE_FLAG_STRING(default_aliuid);
DECLARE_FLAG_STRING(default_access_key_id);
DECLARE_FLAG_STRING(default_access_key);
DECLARE_FLAG_INT32(sls_client_send_timeout);
DECLARE_FLAG_BOOL(sls_client_send_compress);
DECLARE_FLAG_INT32(send_retrytimes);
DECLARE_FLAG_STRING(user_log_config);
DECLARE_FLAG_STRING(ilogtail_config);
DECLARE_FLAG_DOUBLE(loggroup_bytes_inflation);
DECLARE_FLAG_BOOL(ilogtail_discard_old_data);
DECLARE_FLAG_INT32(ilogtail_discard_interval);
DECLARE_FLAG_STRING(app_info_file);
DECLARE_FLAG_INT32(timeout_interval);
DECLARE_FLAG_STRING(default_region_name);
DECLARE_FLAG_STRING(fuse_root_dir);
DECLARE_FLAG_BOOL(enable_root_path_collection);
DECLARE_FLAG_INT32(logtail_alarm_interval);