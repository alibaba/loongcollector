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


#include <Winsock2.h>
#include <direct.h>
#include <fstream>
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

DEFINE_FLAG_STRING(ilogtail_daemon_startup_hints, "hints passed from daemon during startup", "");
DECLARE_FLAG_STRING(ilogtail_config_env_name);
DECLARE_FLAG_STRING(logtail_sys_conf_dir);
DECLARE_FLAG_STRING(check_point_filename);
DECLARE_FLAG_STRING(default_buffer_file_path);
DECLARE_FLAG_STRING(ilogtail_docker_file_path_config);

static void overwrite_community_edition_flags() {
    // support run in installation dir on default
    STRING_FLAG(logtail_sys_conf_dir) = ".";
    STRING_FLAG(check_point_filename) = "checkpoint/logtail_check_point";
    STRING_FLAG(default_buffer_file_path) = "checkpoint";
    STRING_FLAG(ilogtail_docker_file_path_config) = "checkpoint/docker_path_config.json";
}

void do_worker_process() {
    Logger::Instance().InitGlobalLoggers();

    // Initialize Winsock.
    int iResult;
    WSADATA wsaData;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        LOG_FATAL(sLogger, ("Initialize Winsock failed", iResult));
        return;
    }
    LogFileProfiler::GetInstance();

    // Change working dir to ./${ILOGTAIL_VERSION}/.
    std::string processExecutionDir = GetProcessExecutionDir();
    std::string newWorkingDir = processExecutionDir + ILOGTAIL_VERSION;
    AppConfig::GetInstance()->SetProcessExecutionDir(processExecutionDir);
    int chdirRst = _chdir(newWorkingDir.c_str());
    if (0 == chdirRst) {
        LOG_INFO(sLogger, ("Change working dir to", newWorkingDir));
        AppConfig::GetInstance()->SetWorkingDir(newWorkingDir + PATH_SEPARATOR);
    } else {
        // Error, rollback.
        LOG_INFO(sLogger, ("Change working dir to", newWorkingDir)("fail, reason", ErrnoToString(GetErrno())));
        chdirRst = _chdir(GetProcessExecutionDir().c_str());
        LOG_INFO(sLogger, ("Rollback working dir to", GetProcessExecutionDir())("result", chdirRst));
        AppConfig::GetInstance()->SetWorkingDir(GetProcessExecutionDir());
    }

    char* configEnv = getenv(STRING_FLAG(ilogtail_config_env_name).c_str());
    if (configEnv == NULL || strlen(configEnv) == 0) {
        AppConfig::GetInstance()->LoadAppConfig(STRING_FLAG(ilogtail_config));
    } else {
        AppConfig::GetInstance()->LoadAppConfig(configEnv);
    }

    const std::string& intf = AppConfig::GetInstance()->GetBindInterface();
    const std::string& configIP = AppConfig::GetInstance()->GetConfigIP();

    if (configIP.size() > 0) {
        LogFileProfiler::mIpAddr = configIP;
        LogtailMonitor::Instance()->UpdateConstMetric("logtail_ip", GetHostIp());
    } else if (intf.size() > 0)
        LogFileProfiler::mIpAddr = GetHostIp(intf);
    if (LogFileProfiler::mIpAddr.empty()) {
        LogFileProfiler::mIpAddr = GetAnyAvailableIP();
        LOG_INFO(
            sLogger,
            ("IP is still empty, specified interface", intf)("try to get any available IP", LogFileProfiler::mIpAddr));
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

    std::string backTraceStr = GetCrashBackTrace();
    if (backTraceStr.size() > 0) {
        APSARA_LOG_ERROR(sLogger, ("last logtail crash stack", backTraceStr)("stack size", backTraceStr.length()));
        LogtailAlarm::GetInstance()->SendAlarm(LOGTAIL_CRASH_STACK_ALARM, backTraceStr);
    }
    InitCrashBackTrace();

    auto& startupHints = STRING_FLAG(ilogtail_daemon_startup_hints);
    if (!startupHints.empty()) {
        if (startupHints != "Update") {
            LOG_ERROR(sLogger, ("Non-empty startup hints", startupHints));
            LogtailAlarm::GetInstance()->SendAlarm(WINDOWS_WORKER_START_HINTS_ALARM,
                                                   "Non-empty startup hints: " + startupHints);
        } else {
            LOG_INFO(sLogger,
                     ("Logtail restarted because of self-update, "
                      "current version",
                      ILOGTAIL_VERSION));
        }
    }

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

    Json::Value appInfoJson;
    appInfoJson["ip"] = Json::Value(LogFileProfiler::mIpAddr);
    appInfoJson["hostname"] = Json::Value(LogFileProfiler::mHostname);
    appInfoJson["UUID"] = Json::Value(ConfigManager::GetInstance()->GetUUID());
    appInfoJson["instance_id"] = Json::Value(ConfigManager::GetInstance()->GetInstanceId());
    appInfoJson["logtail_version"] = Json::Value(ILOGTAIL_VERSION);
    appInfoJson["git_hash"] = Json::Value(ILOGTAIL_GIT_HASH);
    #define STRINGIFY(x) #x
    #define VERSION_STR(A) "MSVC " STRINGIFY(A)
    #define ILOGTAIL_COMPILER VERSION_STR(_MSC_FULL_VER)
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
            std::cout << "load ilogtail config fail, file content is not valid json: " << STRING_FLAG(ilogtail_config)
                      << std::endl;
            exit(4);
        }
    }

    do_worker_process();
}
