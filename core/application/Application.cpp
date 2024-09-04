// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "application/Application.h"

#ifndef LOGTAIL_NO_TC_MALLOC
#include <gperftools/malloc_extension.h>
#endif

#include <thread>

#include "app_config/AppConfig.h"
#include "checkpoint/CheckPointManager.h"
#include "common/CrashBackTraceUtil.h"
#include "common/Flags.h"
#include "common/MachineInfoUtil.h"
#include "common/RuntimeUtil.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "common/UUIDUtil.h"
#include "common/version.h"
#include "config/ConfigDiff.h"
#include "config/watcher/ConfigWatcher.h"
#include "config_manager/ConfigManager.h"
#include "controller/EventDispatcher.h"
#include "event_handler/LogInput.h"
#include "file_server/FileServer.h"
#include "flusher/sls/DiskBufferWriter.h"
#include "go_pipeline/LogtailPlugin.h"
#include "input/InputFeedbackInterfaceRegistry.h"
#include "logger/Logger.h"
#include "monitor/LogFileProfiler.h"
#include "monitor/MetricExportor.h"
#include "monitor/Monitor.h"
#include "pipeline/InstanceConfigManager.h"
#include "pipeline/PipelineManager.h"
#include "plugin/PluginRegistry.h"
#include "processor/daemon/LogProcess.h"
#include "queue/ExactlyOnceQueueManager.h"
#include "queue/SenderQueueManager.h"
#include "sender/FlusherRunner.h"
#include "sink/http/HttpSink.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#include "config/provider/LegacyConfigProvider.h"
#if defined(__linux__) && !defined(__ANDROID__)
#include "common/LinuxDaemonUtil.h"
#include "shennong/ShennongManager.h"
#include "streamlog/StreamLogManager.h"
#endif
#else
#include "config/provider/CommonConfigProvider.h"
#include "config/provider/LegacyCommonConfigProvider.h"
#endif

DEFINE_FLAG_BOOL(ilogtail_disable_core, "disable core in worker process", true);
DEFINE_FLAG_STRING(ilogtail_config_env_name, "config file path", "ALIYUN_LOGTAIL_CONFIG");
DEFINE_FLAG_STRING(app_info_file, "", "app_info.json");
DEFINE_FLAG_INT32(file_tags_update_interval, "second", 1);
DEFINE_FLAG_INT32(config_scan_interval, "seconds", 10);
DEFINE_FLAG_INT32(profiling_check_interval, "seconds", 60);
DEFINE_FLAG_INT32(tcmalloc_release_memory_interval, "force release memory held by tcmalloc, seconds", 300);
DEFINE_FLAG_INT32(exit_flushout_duration, "exit process flushout duration", 20 * 1000);
DEFINE_FLAG_INT32(queue_check_gc_interval_sec, "30s", 30);
#if defined(__ENTERPRISE__) && defined(__linux__) && !defined(__ANDROID__)
DEFINE_FLAG_BOOL(enable_cgroup, "", true);
#endif


DECLARE_FLAG_BOOL(send_prefer_real_ip);
DECLARE_FLAG_BOOL(global_network_success);

using namespace std;

namespace logtail {

Application::Application() : mStartTime(time(nullptr)) {
}

void Application::Init() {
    // get last crash info
    string backTraceStr = GetCrashBackTrace();
    if (!backTraceStr.empty()) {
        LOG_ERROR(sLogger, ("last logtail crash stack", backTraceStr));
        LogtailAlarm::GetInstance()->SendAlarm(LOGTAIL_CRASH_STACK_ALARM, backTraceStr);
    }
    if (BOOL_FLAG(ilogtail_disable_core)) {
        InitCrashBackTrace();
    }

    // change working dir to ./${ILOGTAIL_VERSION}/
    string processExecutionDir = GetProcessExecutionDir();
    AppConfig::GetInstance()->SetProcessExecutionDir(processExecutionDir);
    string newWorkingDir = processExecutionDir + ILOGTAIL_VERSION;
#ifdef _MSC_VER
    int chdirRst = _chdir(newWorkingDir.c_str());
#else
    int chdirRst = chdir(newWorkingDir.c_str());
#endif
    if (chdirRst == 0) {
        LOG_INFO(sLogger, ("working dir", newWorkingDir));
        AppConfig::GetInstance()->SetWorkingDir(newWorkingDir + "/");
    } else {
        // if change error, try change working dir to ./
#ifdef _MSC_VER
        _chdir(GetProcessExecutionDir().c_str());
#else
        chdir(GetProcessExecutionDir().c_str());
#endif
        LOG_INFO(sLogger, ("working dir", GetProcessExecutionDir()));
        AppConfig::GetInstance()->SetWorkingDir(GetProcessExecutionDir());
    }

    // load ilogtail_config.json
    char* configEnv = getenv(STRING_FLAG(ilogtail_config_env_name).c_str());
    if (configEnv == NULL || strlen(configEnv) == 0) {
        AppConfig::GetInstance()->LoadAppConfig(STRING_FLAG(ilogtail_config));
    } else {
        AppConfig::GetInstance()->LoadAppConfig(configEnv);
    }

    // Initialize basic information: IP, hostname, etc.
    LogFileProfiler::GetInstance();

    // override process related params if designated by user explicitly
    const string& interface = AppConfig::GetInstance()->GetBindInterface();
    const string& configIP = AppConfig::GetInstance()->GetConfigIP();
    if (!configIP.empty()) {
        LogFileProfiler::mIpAddr = configIP;
        LogtailMonitor::GetInstance()->UpdateConstMetric("logtail_ip", GetHostIp());
    } else if (!interface.empty()) {
        LogFileProfiler::mIpAddr = GetHostIp(interface);
        if (LogFileProfiler::mIpAddr.empty()) {
            LOG_WARNING(sLogger,
                        ("failed to get ip from interface", "try to get any available ip")("interface", interface));
        }
    } else if (LogFileProfiler::mIpAddr.empty()) {
        LOG_WARNING(sLogger, ("failed to get ip from hostname or eth0 or bond0", "try to get any available ip"));
    }
    if (LogFileProfiler::mIpAddr.empty()) {
        LogFileProfiler::mIpAddr = GetAnyAvailableIP();
        LOG_INFO(sLogger, ("get available ip succeeded", LogFileProfiler::mIpAddr));
    }

    const string& configHostName = AppConfig::GetInstance()->GetConfigHostName();
    if (!configHostName.empty()) {
        LogFileProfiler::mHostname = configHostName;
        LogtailMonitor::GetInstance()->UpdateConstMetric("logtail_hostname", GetHostName());
    }

    GenerateInstanceId();
    TryGetUUID();

#if defined(__ENTERPRISE__) && defined(__linux__) && !defined(__ANDROID__)
    if (BOOL_FLAG(enable_cgroup)) {
        CreateCGroup();
    }
#endif

    int32_t systemBootTime = AppConfig::GetInstance()->GetSystemBootTime();
    LogFileProfiler::mSystemBootTime = systemBootTime > 0 ? systemBootTime : GetSystemBootTime();

    // generate app_info.json
    Json::Value appInfoJson;
    appInfoJson["ip"] = Json::Value(LogFileProfiler::mIpAddr);
    appInfoJson["hostname"] = Json::Value(LogFileProfiler::mHostname);
    appInfoJson["UUID"] = Json::Value(Application::GetInstance()->GetUUID());
    appInfoJson["instance_id"] = Json::Value(Application::GetInstance()->GetInstanceId());
#ifdef __ENTERPRISE__
    appInfoJson["logtail_version"] = Json::Value(ILOGTAIL_VERSION);
#else
    appInfoJson["logtail_version"] = Json::Value(string(ILOGTAIL_VERSION) + " Community Edition");
    appInfoJson["git_hash"] = Json::Value(ILOGTAIL_GIT_HASH);
    appInfoJson["build_date"] = Json::Value(ILOGTAIL_BUILD_DATE);
#endif
#define STRINGIFY(x) #x
#ifdef _MSC_VER
#define VERSION_STR(A) "MSVC " STRINGIFY(A)
#define ILOGTAIL_COMPILER VERSION_STR(_MSC_FULL_VER)
#else
#define VERSION_STR(A, B, C) "GCC " STRINGIFY(A) "." STRINGIFY(B) "." STRINGIFY(C)
#define ILOGTAIL_COMPILER VERSION_STR(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#endif
    appInfoJson["compiler"] = Json::Value(ILOGTAIL_COMPILER);
    appInfoJson["os"] = Json::Value(LogFileProfiler::mOsDetail);
    appInfoJson["update_time"] = GetTimeStamp(time(NULL), "%Y-%m-%d %H:%M:%S");
    string appInfo = appInfoJson.toStyledString();
    OverwriteFile(GetProcessExecutionDir() + STRING_FLAG(app_info_file), appInfo);
    LOG_INFO(sLogger, ("app info", appInfo));
}

void Application::Start() { // GCOVR_EXCL_START
    LogtailMonitor::GetInstance()->UpdateConstMetric("start_time", GetTimeStamp(time(NULL), "%Y-%m-%d %H:%M:%S"));

#if defined(__ENTERPRISE__) && defined(_MSC_VER)
    InitWindowsSignalObject();
#endif
    BoundedSenderQueueInterface::SetFeedback(ProcessQueueManager::GetInstance());

    HttpSink::GetInstance()->Init();
    FlusherRunner::GetInstance()->Init();

    {
        // add local config dir
        filesystem::path localConfigPath
            = filesystem::path(AppConfig::GetInstance()->GetLogtailSysConfDir()) / "config" / "local";
        error_code ec;
        filesystem::create_directories(localConfigPath, ec);
        if (ec) {
            LOG_WARNING(sLogger,
                        ("failed to create dir for local pipelineconfig",
                         "manual creation may be required")("error code", ec.value())("error msg", ec.message()));
        }
        ConfigWatcher::GetInstance()->AddPipelineSource(localConfigPath.string());
    }
    {
        // add local config dir
        filesystem::path localConfigPath
            = filesystem::path(AppConfig::GetInstance()->GetLogtailSysConfDir()) / "instanceconfig" / "local";
        error_code ec;
        filesystem::create_directories(localConfigPath, ec);
        if (ec) {
            LOG_WARNING(sLogger,
                        ("failed to create dir for local instanceconfig",
                         "manual creation may be required")("error code", ec.value())("error msg", ec.message()));
        }
        ConfigWatcher::GetInstance()->AddInstanceSource(localConfigPath.string());
    }

#ifdef __ENTERPRISE__
    EnterpriseConfigProvider::GetInstance()->Init("enterprise");
    LegacyConfigProvider::GetInstance()->Init("legacy");
#else
    CommonConfigProvider::GetInstance()->Init("common_v2");
    LegacyCommonConfigProvider::GetInstance()->Init("common");
#endif

    LogtailAlarm::GetInstance()->Init();
    LoongCollectorMonitor::GetInstance()->Init();
    LogtailMonitor::GetInstance()->Init();

    PluginRegistry::GetInstance()->LoadPlugins();
    InputFeedbackInterfaceRegistry::GetInstance()->LoadFeedbackInterfaces();

#if defined(__ENTERPRISE__) && defined(__linux__) && !defined(__ANDROID__)
    if (AppConfig::GetInstance()->ShennongSocketEnabled()) {
        ShennongManager::GetInstance()->Init();
    }
#endif

    // If in purage container mode, it means iLogtail is deployed as Daemonset, so plugin base should be loaded since
    // liveness probe relies on it.
    if (AppConfig::GetInstance()->IsPurageContainerMode()) {
        LogtailPlugin::GetInstance()->LoadPluginBase();
    }
    // Actually, docker env config will not work if not in purage container mode, so there is no need to load plugin
    // base if not in purage container mode. However, we still load it here for backward compatability.
    const char* dockerEnvConfig = getenv("ALICLOUD_LOG_DOCKER_ENV_CONFIG");
    if (dockerEnvConfig != NULL && strlen(dockerEnvConfig) > 0
        && (dockerEnvConfig[0] == 't' || dockerEnvConfig[0] == 'T')) {
        LogtailPlugin::GetInstance()->LoadPluginBase();
    }

    LogProcess::GetInstance()->Start();

    time_t curTime = 0, lastProfilingCheckTime = 0, lastConfigCheckTime = 0, lastUpdateMetricTime = 0,
           lastCheckTagsTime = 0, lastQueueGCTime = 0;
#ifndef LOGTAIL_NO_TC_MALLOC
    time_t lastTcmallocReleaseMemTime = 0;
#endif
    while (true) {
        curTime = time(NULL);
        if (curTime - lastCheckTagsTime >= INT32_FLAG(file_tags_update_interval)) {
            AppConfig::GetInstance()->UpdateFileTags();
            lastCheckTagsTime = curTime;
        }
        if (curTime - lastConfigCheckTime >= INT32_FLAG(config_scan_interval)) {
            PipelineConfigDiff pipelineConfigDiff = ConfigWatcher::GetInstance()->CheckPipelineConfigDiff();
            if (!pipelineConfigDiff.IsEmpty()) {
                PipelineManager::GetInstance()->UpdatePipelines(pipelineConfigDiff);
            }
            InstanceConfigDiff instanceConfigDiff = ConfigWatcher::GetInstance()->CheckInstanceConfigDiff();
            if (!instanceConfigDiff.IsEmpty()) {
                InstanceConfigManager::GetInstance()->UpdateInstanceConfigs(instanceConfigDiff);
            }
            lastConfigCheckTime = curTime;
        }
        if (curTime - lastProfilingCheckTime >= INT32_FLAG(profiling_check_interval)) {
            LogFileProfiler::GetInstance()->SendProfileData();
            MetricExportor::GetInstance()->PushMetrics(false);
            lastProfilingCheckTime = curTime;
        }
#ifndef LOGTAIL_NO_TC_MALLOC
        if (curTime - lastTcmallocReleaseMemTime >= INT32_FLAG(tcmalloc_release_memory_interval)) {
            MallocExtension::instance()->ReleaseFreeMemory();
            lastTcmallocReleaseMemTime = curTime;
        }
#endif
        if (curTime - lastQueueGCTime >= INT32_FLAG(queue_check_gc_interval_sec)) {
            ExactlyOnceQueueManager::GetInstance()->ClearTimeoutQueues();
            // this should be called in the same thread as config update
            SenderQueueManager::GetInstance()->ClearUnusedQueues();
            lastQueueGCTime = curTime;
        }
        if (curTime - lastUpdateMetricTime >= 40) {
            CheckCriticalCondition(curTime);
            lastUpdateMetricTime = curTime;
        }
        if (mSigTermSignalFlag.load()) {
            LOG_INFO(sLogger, ("received SIGTERM signal", "exit process"));
            Exit();
        }
#if defined(__ENTERPRISE__) && defined(_MSC_VER)
        SyncWindowsSignalObject();
#endif
        // 过渡使用
        EventDispatcher::GetInstance()->DumpCheckPointPeriod(curTime);

        if (ConfigManager::GetInstance()->IsUpdateContainerPaths()) {
            FileServer::GetInstance()->Pause();
            FileServer::GetInstance()->Resume();
        }

        // destruct event handlers here so that it will not block file reading task
        ConfigManager::GetInstance()->DeleteHandlers();

        this_thread::sleep_for(chrono::seconds(1));
    }
} // GCOVR_EXCL_STOP

void Application::GenerateInstanceId() {
    mInstanceId = CalculateRandomUUID() + "_" + LogFileProfiler::mIpAddr + "_" + ToString(mStartTime);
}

bool Application::TryGetUUID() {
    mUUIDThread = thread([this] { GetUUIDThread(); });
    // wait 1000 ms
    for (int i = 0; i < 100; ++i) {
        this_thread::sleep_for(chrono::milliseconds(10));
        if (!GetUUID().empty()) {
            return true;
        }
    }
    return false;
}

void Application::Exit() {
#if defined(__ENTERPRISE__) && defined(__linux__) && !defined(__ANDROID__)
    if (AppConfig::GetInstance()->ShennongSocketEnabled()) {
        ShennongManager::GetInstance()->Stop();
    }
#endif

    PipelineManager::GetInstance()->StopAllPipelines();

    PluginRegistry::GetInstance()->UnloadPlugins();

#ifdef __ENTERPRISE__
    EnterpriseConfigProvider::GetInstance()->Stop();
    LegacyConfigProvider::GetInstance()->Stop();
#else
    CommonConfigProvider::GetInstance()->Stop();
    LegacyCommonConfigProvider::GetInstance()->Stop();
#endif

    LogtailMonitor::GetInstance()->Stop();
    LoongCollectorMonitor::GetInstance()->Stop();
    LogtailAlarm::GetInstance()->Stop();
    // from now on, alarm should not be used.

    FlusherRunner::GetInstance()->Stop();
    HttpSink::GetInstance()->Stop();

    // TODO: make it common
    FlusherSLS::RecycleResourceIfNotUsed();

#if defined(_MSC_VER)
    ReleaseWindowsSignalObject();
#endif
    LOG_INFO(sLogger, ("exit", "bye!"));
    exit(0);
}

void Application::CheckCriticalCondition(int32_t curTime) {
#ifdef __ENTERPRISE__
    int32_t lastGetConfigTime = EnterpriseConfigProvider::GetInstance()->GetLastConfigGetTime();
    // force to exit if config update thread is block more than 1 hour
    if (lastGetConfigTime > 0 && curTime - lastGetConfigTime > 3600) {
        LOG_ERROR(sLogger, ("last config get time is too old", lastGetConfigTime)("prepare force exit", ""));
        LogtailAlarm::GetInstance()->SendAlarm(
            LOGTAIL_CRASH_ALARM, "last config get time is too old: " + ToString(lastGetConfigTime) + " force exit");
        LogtailAlarm::GetInstance()->ForceToSend();
        sleep(10);
        _exit(1);
    }
#endif
    // if network is fail in 2 hours, force exit (for ant only)
    // work around for no network when docker start
    if (BOOL_FLAG(send_prefer_real_ip) && !BOOL_FLAG(global_network_success) && curTime - mStartTime > 7200) {
        LOG_ERROR(sLogger, ("network is fail", "prepare force exit"));
        LogtailAlarm::GetInstance()->SendAlarm(LOGTAIL_CRASH_ALARM,
                                               "network is fail since " + ToString(mStartTime) + " force exit");
        LogtailAlarm::GetInstance()->ForceToSend();
        sleep(10);
        _exit(1);
    }
}

bool Application::GetUUIDThread() {
    string uuid = CalculateRandomUUID();
    SetUUID(uuid);
    return true;
}

} // namespace logtail
