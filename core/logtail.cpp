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

#include <sys/wait.h>
#include <sys/socket.h>
#include <syslog.h>
#include <sys/resource.h>
#include <fstream>
#include <vector>
#include <errno.h>
#include "common/version.h"
#include "common/util.h"
#include "common/LogtailCommonFlags.h"
#include "common/RuntimeUtil.h"
#include "common/TimeUtil.h"
#include "common/FileSystemUtil.h"
#include "common/CrashBackTraceUtil.h"
#include "common/MachineInfoUtil.h"
#include "common/ErrorUtil.h"
#include "common/GlobalPara.h"
#include "logger/Logger.h"
#ifdef LOGTAIL_RUNTIME_PLUGIN
#include "plugin/LogtailRuntimePlugin.h"
#endif
#include "plugin/LogtailPlugin.h"
#include "config_manager/ConfigManager.h"
#include "checkpoint/CheckPointManager.h"
#include "processor/LogFilter.h"
#include "controller/EventDispatcher.h"
#include "monitor/Monitor.h"
#include "sender/Sender.h"
#include "profiler/LogtailAlarm.h"
#include "profiler/LogFileProfiler.h"
#include "profiler/LogIntegrity.h"
#include "profiler/LogLineCount.h"
#include "app_config/AppConfig.h"
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

DEFINE_FLAG_BOOL(ilogtail_disable_core, "disable core in worker process", true);
DEFINE_FLAG_INT32(fork_interval, "fork dispatcher process interval", 10);
DECLARE_FLAG_INT32(max_open_files_limit);
DECLARE_FLAG_INT32(max_reader_open_files);
DECLARE_FLAG_STRING(ilogtail_config_env_name);
DECLARE_FLAG_STRING(logtail_sys_conf_dir);
DECLARE_FLAG_STRING(check_point_filename);
DECLARE_FLAG_STRING(default_buffer_file_path);
DECLARE_FLAG_STRING(ilogtail_docker_file_path_config);

void HandleSighupSignal(int signum, siginfo_t* info, void* context) {
    ConfigManager::GetInstance()->SetMappingPathsChanged();
}

void HandleSigtermSignal(int signum, siginfo_t* info, void* context) {
    LogtailGlobalPara::Instance()->SetSigtermFlag(true);
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
}

// Main routine of worker process.
void do_worker_process() {
    Logger::Instance().InitGlobalLoggers();

    struct sigaction sigtermSig;
    sigemptyset(&sigtermSig.sa_mask);
    sigtermSig.sa_sigaction = HandleSigtermSignal;
    sigtermSig.sa_flags = SA_SIGINFO;
    if (sigaction(SIGTERM, &sigtermSig, NULL) < 0) {
        APSARA_LOG_ERROR(sLogger, ("install SIGTERM", "fail"));
        exit(5);
    }
    if (sigaction(SIGINT, &sigtermSig, NULL) < 0) {
        APSARA_LOG_ERROR(sLogger, ("install SIGINT", "fail"));
        exit(5);
    }

    struct sigaction sighupSig;
    sigemptyset(&sighupSig.sa_mask);
    sighupSig.sa_sigaction = HandleSighupSignal;
    sighupSig.sa_flags = SA_SIGINFO;
    if (sigaction(SIGHUP, &sighupSig, NULL) < 0) {
        APSARA_LOG_ERROR(sLogger, ("install SIGHUP", "fail"));
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
    // Initialize basic information: IP, hostname, etc.
    LogFileProfiler::GetInstance();

    // change working dir to ./${ILOGTAIL_VERSION}/
    std::string processExecutionDir = GetProcessExecutionDir();
    std::string newWorkingDir = processExecutionDir + ILOGTAIL_VERSION;
    AppConfig::GetInstance()->SetProcessExecutionDir(processExecutionDir);
    int chdirRst = chdir(newWorkingDir.c_str());
    if (chdirRst == 0) {
        APSARA_LOG_INFO(sLogger, ("change working dir", newWorkingDir));
        AppConfig::GetInstance()->SetWorkingDir(newWorkingDir + "/");
    } else {
        // if change error, try change working dir to ./
        APSARA_LOG_INFO(sLogger, ("change working dir", newWorkingDir)("fail, reason", ErrnoToString(GetErrno())));
        chdirRst = chdir(GetProcessExecutionDir().c_str());
        APSARA_LOG_INFO(sLogger, ("change working dir", GetProcessExecutionDir())("result", chdirRst));
        AppConfig::GetInstance()->SetWorkingDir(GetProcessExecutionDir());
    }

    overwrite_community_edition_flags();

    char* configEnv = getenv(STRING_FLAG(ilogtail_config_env_name).c_str());
    if (configEnv == NULL || strlen(configEnv) == 0) {
        AppConfig::GetInstance()->LoadAppConfig(STRING_FLAG(ilogtail_config));
    } else {
        AppConfig::GetInstance()->LoadAppConfig(configEnv);
    }

    const std::string& interface = AppConfig::GetInstance()->GetBindInterface();
    const std::string& configIP = AppConfig::GetInstance()->GetConfigIP();

    if (configIP.size() > 0) {
        LogFileProfiler::mIpAddr = configIP;
        LogtailMonitor::Instance()->UpdateConstMetric("logtail_ip", GetHostIp());
    } else if (interface.size() > 0)
        LogFileProfiler::mIpAddr = GetHostIp(interface);
    if (LogFileProfiler::mIpAddr.empty()) {
        LogFileProfiler::mIpAddr = GetAnyAvailableIP();
        APSARA_LOG_INFO(sLogger,
                        ("IP is still empty, specified interface", interface)("try to get any available IP",
                                                                              LogFileProfiler::mIpAddr));
    }

    const std::string& configHostName = AppConfig::GetInstance()->GetConfigHostName();
    if (configHostName.size() > 0) {
        LogFileProfiler::mHostname = configHostName;
        LogtailMonitor::Instance()->UpdateConstMetric("logtail_hostname", GetHostName());
    }

    int32_t systemBootTime = AppConfig::GetInstance()->GetSystemBootTime();
    LogFileProfiler::mSystemBootTime = systemBootTime > 0 ? systemBootTime : GetSystemBootTime();

    LogtailMonitor::Instance()->UpdateConstMetric("start_time", GetTimeStamp(time(NULL), "%Y-%m-%d %H:%M:%S"));

    // use a thread to get uuid
    if (!ConfigManager::GetInstance()->TryGetUUID()) {
        APSARA_LOG_INFO(sLogger, ("get none dmi uuid", "maybe this is a docker runtime"));
    }

#ifdef LOGTAIL_RUNTIME_PLUGIN
    LogtailRuntimePlugin::GetInstance()->LoadPluginBase();
#endif

    // load local config first
    ConfigManager::GetInstance()->GetLocalConfigUpdate();
    ConfigManager::GetInstance()->LoadConfig(AppConfig::GetInstance()->GetUserConfigPath());
    ConfigManager::GetInstance()->LoadDockerConfig();
    // mNameCoonfigMap is empty, configExistFlag is false
    bool configExistFlag = !ConfigManager::GetInstance()->GetAllConfig().empty();

    //set max open file limit
    struct rlimit rlimMaxOpenFiles;
    rlimMaxOpenFiles.rlim_cur = rlimMaxOpenFiles.rlim_max = INT32_FLAG(max_open_files_limit);
    if (0 != setrlimit(RLIMIT_NOFILE, &rlimMaxOpenFiles)) {
        APSARA_LOG_ERROR(sLogger,
                         ("set resource limit error, open file limit",
                          INT32_FLAG(max_open_files_limit))("reason", ErrnoToString(GetErrno())));
        if (getrlimit(RLIMIT_NOFILE, &rlimMaxOpenFiles) == 0) {
            APSARA_LOG_ERROR(
                sLogger,
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
            APSARA_LOG_ERROR(sLogger,
                             ("get resource limit error, "
                              "set max open files to 800",
                              ErrnoToString(GetErrno())));
            INT32_FLAG(max_open_files_limit) = 1024;
            INT32_FLAG(max_reader_open_files) = (int32_t)(1024 * 0.8);
        }
    }

    std::string backTraceStr = GetCrashBackTrace();
    if (backTraceStr.size() > 0) {
        APSARA_LOG_ERROR(sLogger, ("last logtail crash stack", backTraceStr));
        LogtailAlarm::GetInstance()->SendAlarm(LOGTAIL_CRASH_STACK_ALARM, backTraceStr);
    }

    InitCrashBackTrace();

    LogtailMonitor::Instance()->InitMonitor();
    LogFilter::Instance()->InitFilter(STRING_FLAG(user_log_config));
    Sender::Instance()->InitSender();
    LogtailPlugin* pPlugin = LogtailPlugin::GetInstance();
    if (pPlugin->LoadPluginBase()) {
        pPlugin->Resume();
    }
    CheckPointManager::Instance()->LoadCheckPoint();

    // added by xianzhi(bowen.gbw@antfin.com)
    // read local data_integrity json file and line count file
    LogIntegrity::GetInstance()->ReloadIntegrityDataFromLocalFile();
    LogLineCount::GetInstance()->ReloadLineCountDataFromLocalFile();

    // Collect application information and write them to app_info.json.
    Json::Value appInfoJson;
    appInfoJson["ip"] = Json::Value(LogFileProfiler::mIpAddr);
    appInfoJson["hostname"] = Json::Value(LogFileProfiler::mHostname);
    appInfoJson["UUID"] = Json::Value(ConfigManager::GetInstance()->GetUUID());
    appInfoJson["instance_id"] = Json::Value(ConfigManager::GetInstance()->GetInstanceId());
    appInfoJson["logtail_version"] = Json::Value(std::string(ILOGTAIL_VERSION) + " Community Edition");
    appInfoJson["git_hash"] = Json::Value(ILOGTAIL_GIT_HASH);
    #define STRINGIFY(x) #x
    #define VERSION_STR(A,B,C) "GCC " STRINGIFY(A) "." STRINGIFY(B) "." STRINGIFY(C)
    #define ILOGTAIL_COMPILER VERSION_STR(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
    appInfoJson["compiler"] = Json::Value(ILOGTAIL_COMPILER);
    appInfoJson["build_date"] = Json::Value(ILOGTAIL_BUILD_DATE);
    appInfoJson["os"] = Json::Value(LogFileProfiler::mOsDetail);
    appInfoJson["update_time"] = GetTimeStamp(time(NULL), "%Y-%m-%d %H:%M:%S");
    std::string appInfo = appInfoJson.toStyledString();
    OverwriteFile(GetProcessExecutionDir() + STRING_FLAG(app_info_file), appInfo);
    APSARA_LOG_INFO(sLogger, ("Logtail started, appInfo", appInfo));

    ConfigManager::GetInstance()->InitUpdateConfig(configExistFlag);
    ConfigManager::GetInstance()->RegisterHandlers();
    EventDispatcher::GetInstance()->AddExistedCheckPointFileEvents();

    // [Main thread] Run the Dispatch routine.
    EventDispatcher::GetInstance()->Dispatch();
}

int main(int argc, char** argv) {
    gflags::SetUsageMessage(std::string("The Lightweight Collector of SLS in Alibaba Cloud\nUsage: ./ilogtail [OPTION]"));
    gflags::SetVersionString(std::string(ILOGTAIL_VERSION) + " Community Edition");
    google::ParseCommandLineFlags(&argc, &argv, true);
    // check addr config
    Json::Value addrConfigJson;
    ParseConfResult addrRes = ParseConfig(STRING_FLAG(ilogtail_config), addrConfigJson);
    if (addrRes == CONFIG_INVALID_FORMAT) {
        // Check if the file is existing and empty? if true, do not exit.
        if (!IsEmptyConfigJSONFile(STRING_FLAG(ilogtail_config))) {
            syslog(LOG_WARNING,
                   (std::string("load ilogtail config fail, file content is not "
                                "valid json: ")
                    + STRING_FLAG(ilogtail_config))
                       .c_str());
            exit(4);
        }
    }

    if (setenv("TCMALLOC_RELEASE_RATE", "10.0", 1) == -1) {
        exit(3);
    }
    do_worker_process();

    return 0;
}
