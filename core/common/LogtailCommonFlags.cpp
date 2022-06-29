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

#include "LogtailCommonFlags.h"

// Windows only has polling, give a bigger tail limit.
#if defined(__linux__)
DEFINE_FLAG_INT32(default_tail_limit_kb,
                  "when first open file, if offset little than this value, move offset to beginning, KB",
                  1024);
#elif defined(_MSC_VER)
DEFINE_FLAG_INT32(default_tail_limit_kb,
                  "when first open file, if offset little than this value, move offset to beginning, KB",
                  1024 * 50);
#endif
DEFINE_FLAG_INT32(monitor_interval, "program monitor interval, seconds", 30);
DEFINE_FLAG_DOUBLE(cpu_usage_up_limit, "cpu usage upper limit, cores", 2.0);
DEFINE_FLAG_DOUBLE(pub_cpu_usage_up_limit, "cpu usage upper limit, cores", 0.4);
DEFINE_FLAG_INT64(memory_usage_up_limit, "memory usage upper limit, MB", 2 * 1024);
DEFINE_FLAG_INT64(pub_memory_usage_up_limit, "memory usage upper limit, MB", 200);
DEFINE_FLAG_INT32(default_max_inotify_watch_num, "the max allowed inotify watch dir number", 3000);
DEFINE_FLAG_INT32(cpu_limit_num, "cpu violate limit num before shutdown", 10);
DEFINE_FLAG_INT32(mem_limit_num, "memory violate limit num before shutdown", 10);
DEFINE_FLAG_INT32(batch_send_interval, "batch sender interval (second)(default 3)", 3);
DEFINE_FLAG_INT32(batch_send_metric_size, "batch send matric size limit(bytes)(default 256)", 256 * 1024);
DEFINE_FLAG_INT32(max_holded_data_size,
                  "for every id and metric name, the max data size can be holded in memory (default 2MB)",
                  512 * 1024);
DEFINE_FLAG_INT32(pub_max_holded_data_size,
                  "for every id and metric name, the max data size can be holded in memory (default 2MB)",
                  512 * 1024);
DEFINE_FLAG_STRING(logtail_send_address,
                   "the target address to which to send the log result",
                   "http://sls.aliyun-inc.com");
DEFINE_FLAG_STRING(logtail_config_address,
                   "the target address to which to get config update",
                   "http://config.sls.aliyun-inc.com");
DEFINE_FLAG_INT32(ilogtail_max_epoll_events, "the max events number in epoll", 10000);
DEFINE_FLAG_INT32(log_expire_time, "log expire time", 24 * 3600);
DEFINE_FLAG_INT32(buffer_check_period, "check logtail local storage buffer period", 60);
DEFINE_FLAG_BOOL(default_secondary_storage, "default strategy whether enable secondary storage", false);
DEFINE_FLAG_INT32(ilogtail_discard_interval, "if the data is old than the interval, it will be discard", 43200);
DEFINE_FLAG_BOOL(ilogtail_discard_old_data, "if discard the old data flag", true);
DEFINE_FLAG_STRING(user_log_config,
                   "the configuration file storing user's log collecting parameter",
                   "user_log_config.json");
DEFINE_FLAG_STRING(ilogtail_config,
                   "set dataserver & configserver address; (optional)set cpu,mem,bufflerfile,buffermap and etc.",
                   "ilogtail_config.json");
DEFINE_FLAG_INT32(ilogtail_epoll_wait_events, "epoll_wait event number", 100);
DEFINE_FLAG_INT64(max_logtail_writer_packet_size,
                  "max packet size thourgh domain socket in shennong agent",
                  10000000); // 10M
DEFINE_FLAG_STRING(logtail_profile_snapshot, "reader profile on local disk", "logtail_profile_snapshot");
DEFINE_FLAG_STRING(logtail_line_count_snapshot, "line count file on local disk", "logtail_line_count_snapshot.json");
DEFINE_FLAG_STRING(logtail_integrity_snapshot, "integrity file on local disk", "logtail_integrity_snapshot.json");
DEFINE_FLAG_STRING(logtail_status_snapshot, "status on local disk", "logtail_status_snapshot");
DEFINE_FLAG_INT32(profile_data_send_interval, "interval of send LogFile/DomainSocket profile data, seconds", 600);
DEFINE_FLAG_STRING(local_machine_uuid, "use this value if not empty, for ut/debug", "");
DEFINE_FLAG_STRING(user_defined_id_file, "", "user_defined_id");
DEFINE_FLAG_STRING(logtail_sys_conf_users_dir, "", "users");
DEFINE_FLAG_INT32(sls_client_send_timeout, "timeout time of one operation for SlsClient", 15);
DEFINE_FLAG_BOOL(sls_client_send_compress, "whether compresses the data or not when put data", true);
DEFINE_FLAG_INT32(send_retrytimes, "how many times should retry if PostLogStoreLogs operation fail", 3);
DEFINE_FLAG_INT32(default_StreamLog_tcp_port, "", 11111);
DEFINE_FLAG_INT32(default_StreamLog_poll_size_in_mb, "", 50);
DEFINE_FLAG_INT32(default_StreamLog_recv_size_each_call, "<= 1Mb", 1024);
DEFINE_FLAG_INT64(default_StreamLog_fd_send_buffer, "", 512 * 1024);
DEFINE_FLAG_INT64(default_StreamLog_fd_send_timeout, "", 10 * 1000 * 1000);
DEFINE_FLAG_INT32(StreamLog_line_max_length, "", 10 * 1024 * 1024);
DEFINE_FLAG_DOUBLE(loggroup_bytes_inflation, "", 1.2);
DEFINE_FLAG_STRING(app_info_file, "", "app_info.json");
DEFINE_FLAG_INT32(timeout_interval, "the time interval that an inactive dir being timeout, seconds", 900);
DEFINE_FLAG_STRING(default_region_name,
                   "for compatible with old user_log_config.json or old config server",
                   "__default_region__");
DEFINE_FLAG_STRING(ilogtail_config_env_name, "config file path", "ALIYUN_LOGTAIL_CONFIG");
DEFINE_FLAG_STRING(ilogtail_aliuid_env_name, "aliuid", "ALIYUN_LOGTAIL_USER_ID");
DEFINE_FLAG_STRING(ilogtail_user_defined_id_env_name, "user defined id", "ALIYUN_LOGTAIL_USER_DEFINED_ID");
DEFINE_FLAG_STRING(fuse_root_dir, "root dir for fuse file polling", "/home/admin/logs");
DEFINE_FLAG_INT32(fuse_dir_max_depth, "max depth from fuse root dir", 100);
DEFINE_FLAG_INT32(fuse_file_max_count, "max file total count from fuse root dir", 10000);
DEFINE_FLAG_BOOL(enable_root_path_collection, "", false);
