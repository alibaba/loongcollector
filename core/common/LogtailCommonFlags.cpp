// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/LogtailCommonFlags.h"


// app config
DEFINE_FLAG_STRING(ilogtail_config,
                   "set dataserver & configserver address; (optional)set cpu,mem,bufflerfile,buffermap and etc.",
                   "ilogtail_config.json");
DEFINE_FLAG_BOOL(enable_full_drain_mode, "", false);
DEFINE_FLAG_INT32(cpu_limit_num, "cpu violate limit num before shutdown", 10);
DEFINE_FLAG_INT32(mem_limit_num, "memory violate limit num before shutdown", 10);
DEFINE_FLAG_DOUBLE(cpu_usage_up_limit, "cpu usage upper limit, cores", 2.0);
DEFINE_FLAG_INT64(memory_usage_up_limit, "memory usage upper limit, MB", 2 * 1024);

// epoll
DEFINE_FLAG_INT32(ilogtail_epoll_time_out, "default time out is 1s", 1);
DEFINE_FLAG_INT32(ilogtail_epoll_wait_events, "epoll_wait event number", 100);
DEFINE_FLAG_INT32(ilogtail_max_epoll_events, "the max events number in epoll", 10000);

// sls sender
DEFINE_FLAG_BOOL(sls_client_send_compress, "whether compresses the data or not when put data", true);
DEFINE_FLAG_STRING(default_region_name,
                   "for compatible with old user_log_config.json or old config server",
                   "__default_region__");

// profile
DEFINE_FLAG_STRING(logtail_profile_aliuid, "default user's aliuid", "");

// monitor
DEFINE_FLAG_INT32(monitor_interval, "program monitor interval, seconds", 30);

// process
DEFINE_FLAG_BOOL(ilogtail_discard_old_data, "if discard the old data flag", true);
DEFINE_FLAG_INT32(ilogtail_discard_interval, "if the data is old than the interval, it will be discard", 43200);

// file source
DEFINE_FLAG_BOOL(enable_root_path_collection, "", false);
DEFINE_FLAG_INT32(timeout_interval, "the time interval that an inactive dir being timeout, seconds", 900);

#if defined(_MSC_VER)
DEFINE_FLAG_STRING(default_container_host_path, "", "C:\\logtail_host");
#else
DEFINE_FLAG_STRING(default_container_host_path, "", "/logtail_host");
#endif

// dir
DEFINE_FLAG_STRING(conf_dir, "loongcollector config dir", "conf");
DEFINE_FLAG_STRING(logs_dir, "loongcollector log dir", "log");
DEFINE_FLAG_STRING(data_dir, "loongcollector data dir", "data");
DEFINE_FLAG_STRING(run_dir, "loongcollector run dir", "run");
DEFINE_FLAG_STRING(third_party_dir, "loongcollector third party dir", "thirdparty");
