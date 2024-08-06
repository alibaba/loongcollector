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

#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include "application/Application.h"
#include "common/ErrorUtil.h"
#include "common/Flags.h"
#include "common/version.h"
#include "logger/Logger.h"

using namespace logtail;

#ifdef ENABLE_COMPATIBLE_MODE
extern "C" {
#include <string.h>
asm(".symver memcpy, memcpy@GLIBC_2.2.5");
void* __wrap_memcpy(void* dest, const void* src, size_t n) {
    return memcpy(dest, src, n);
}
}
#endif

DECLARE_FLAG_BOOL(ilogtail_disable_core);
DECLARE_FLAG_INT32(max_open_files_limit);
DECLARE_FLAG_INT32(max_reader_open_files);
DECLARE_FLAG_STRING(logtail_sys_conf_dir);
DECLARE_FLAG_STRING(check_point_filename);
DECLARE_FLAG_STRING(default_buffer_file_path);
DECLARE_FLAG_STRING(ilogtail_docker_file_path_config);
DECLARE_FLAG_STRING(metrics_report_method);
DECLARE_FLAG_INT32(data_server_port);
DECLARE_FLAG_BOOL(enable_env_ref_in_config);
DECLARE_FLAG_BOOL(enable_sls_metrics_format);
DECLARE_FLAG_BOOL(enable_containerd_upper_dir_detect);

void HandleSigtermSignal(int signum, siginfo_t* info, void* context) {
    LOG_INFO(sLogger, ("received signal", "SIGTERM"));
    Application::GetInstance()->SetSigTermSignalFlag(true);
}

void disable_core(void) {
    struct rlimit rlim;
    rlim.rlim_cur = rlim.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &rlim);
}

void enable_core(void) {
    struct rlimit rlim_old;
    struct rlimit rlim_new;
    if (getrlimit(RLIMIT_CORE, &rlim_old) == 0) {
        rlim_new.rlim_cur = rlim_new.rlim_max = 1024 * 1024 * 1024;
        if (setrlimit(RLIMIT_CORE, &rlim_new) != 0) {
            rlim_new.rlim_cur = rlim_old.rlim_cur;
            rlim_new.rlim_max = rlim_old.rlim_max;
            (void)setrlimit(RLIMIT_CORE, &rlim_new);
        }
    }
}

static void overwrite_community_edition_flags() {
    // support run in installation dir on default
    STRING_FLAG(logtail_sys_conf_dir) = ".";
    STRING_FLAG(check_point_filename) = "checkpoint/logtail_check_point";
    STRING_FLAG(default_buffer_file_path) = "checkpoint";
    STRING_FLAG(ilogtail_docker_file_path_config) = "checkpoint/docker_path_config.json";
    STRING_FLAG(metrics_report_method) = "";
    INT32_FLAG(data_server_port) = 443;
    BOOL_FLAG(enable_env_ref_in_config) = true;
    BOOL_FLAG(enable_containerd_upper_dir_detect) = true;
    BOOL_FLAG(enable_sls_metrics_format) = false;
}

// Main routine of worker process.
void do_worker_process() {
    Logger::Instance().InitGlobalLoggers();

    struct sigaction sigtermSig;
    sigemptyset(&sigtermSig.sa_mask);
    sigtermSig.sa_sigaction = HandleSigtermSignal;
    sigtermSig.sa_flags = SA_SIGINFO;
    if (sigaction(SIGTERM, &sigtermSig, NULL) < 0) {
        LOG_ERROR(sLogger, ("install SIGTERM", "fail"));
        exit(5);
    }
    if (sigaction(SIGINT, &sigtermSig, NULL) < 0) {
        LOG_ERROR(sLogger, ("install SIGINT", "fail"));
        exit(5);
    }

#ifndef LOGTAIL_NO_TC_MALLOC
    if (BOOL_FLAG(ilogtail_disable_core)) {
        disable_core();
    } else {
        enable_core();
    }
#else
    enable_core();
#endif

    overwrite_community_edition_flags();

    // set max open file limit
    struct rlimit rlimMaxOpenFiles;
    rlimMaxOpenFiles.rlim_cur = rlimMaxOpenFiles.rlim_max = INT32_FLAG(max_open_files_limit);
    if (0 != setrlimit(RLIMIT_NOFILE, &rlimMaxOpenFiles)) {
        LOG_ERROR(sLogger,
                  ("set resource limit error, open file limit",
                   INT32_FLAG(max_open_files_limit))("reason", ErrnoToString(GetErrno())));
        if (getrlimit(RLIMIT_NOFILE, &rlimMaxOpenFiles) == 0) {
            LOG_ERROR(sLogger,
                      ("this process's resource limit ", rlimMaxOpenFiles.rlim_cur)("max", rlimMaxOpenFiles.rlim_max));
            if (rlimMaxOpenFiles.rlim_max > (rlim_t)INT32_FLAG(max_open_files_limit)) {
                rlimMaxOpenFiles.rlim_max = INT32_FLAG(max_open_files_limit);
            }
            if (rlimMaxOpenFiles.rlim_max < (rlim_t)100) {
                rlimMaxOpenFiles.rlim_max = 100;
            }
            INT32_FLAG(max_open_files_limit) = rlimMaxOpenFiles.rlim_max;
            INT32_FLAG(max_reader_open_files) = (int32_t)(INT32_FLAG(max_open_files_limit) * 0.8);
        } else {
            LOG_ERROR(sLogger,
                      ("get resource limit error, "
                       "set max open files to 800",
                       ErrnoToString(GetErrno())));
            INT32_FLAG(max_open_files_limit) = 1024;
            INT32_FLAG(max_reader_open_files) = (int32_t)(1024 * 0.8);
        }
    }

    Application::GetInstance()->Init();
    Application::GetInstance()->Start();
}

int main(int argc, char** argv) {
    gflags::SetUsageMessage(
        std::string("The Lightweight Collector of SLS in Alibaba Cloud\nUsage: ./ilogtail [OPTION]"));
    gflags::SetVersionString(std::string(ILOGTAIL_VERSION) + " Community Edition");
    if (argc != 0) {
        google::ParseCommandLineFlags(&argc, &argv, true);
    }

    if (setenv("TCMALLOC_RELEASE_RATE", "10.0", 1) == -1) {
        exit(3);
    }

    do_worker_process();

    return 0;
}
