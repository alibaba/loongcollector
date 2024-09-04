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

#include "Monitor.h"
#if defined(__linux__)
#include <asm/param.h>
#include <unistd.h>
#elif defined(_MSC_VER)
#include <Psapi.h>
#endif
#include <fstream>
#include <functional>

#include "app_config/AppConfig.h"
#include "common/Constants.h"
#include "common/DevInode.h"
#include "common/ExceptionBase.h"
#include "common/LogtailCommonFlags.h"
#include "common/MachineInfoUtil.h"
#include "common/RuntimeUtil.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "common/version.h"
#include "config_manager/ConfigManager.h"
#include "event_handler/LogInput.h"
#include "flusher/sls/FlusherSLS.h"
#include "go_pipeline/LogtailPlugin.h"
#include "log_pb/sls_logs.pb.h"
#include "logger/Logger.h"
#include "monitor/LogFileProfiler.h"
#include "monitor/LogtailAlarm.h"
#include "sender/FlusherRunner.h"
#if defined(__linux__) && !defined(__ANDROID__)
#include "ObserverManager.h"
#endif
#include "application/Application.h"
#include "sdk/Common.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif
#include "pipeline/PipelineManager.h"
#include "profile_sender/ProfileSender.h"

using namespace std;
using namespace sls_logs;

DEFINE_FLAG_BOOL(logtail_dump_monitor_info, "enable to dump Logtail monitor info (CPU, mem)", false);
DECLARE_FLAG_BOOL(send_prefer_real_ip);
DECLARE_FLAG_BOOL(check_profile_region);

namespace logtail {

inline void CpuStat::Reset() {
#if defined(__linux__)
    mUserTime = 0;
    mSysTime = 0;
    mSysTotalTime = GetCurrentTimeInMilliSeconds();
#elif defined(_MSC_VER)
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    mNumProcessors = sysInfo.dwNumberOfProcessors;
    FILETIME ftime, fsys, fuser;
    GetSystemTimeAsFileTime(&ftime);
    memcpy(&mLastCPU, &ftime, sizeof(FILETIME));
    mSelf = GetCurrentProcess();
    GetProcessTimes(mSelf, &ftime, &ftime, &fsys, &fuser);
    memcpy(&mLastSysCPU, &fsys, sizeof(FILETIME));
    memcpy(&mLastUserCPU, &fuser, sizeof(FILETIME));
#endif

    mViolateNum = 0;
    mCpuUsage = 0;
}

LogtailMonitor::LogtailMonitor() = default;

LogtailMonitor* LogtailMonitor::GetInstance() {
    static LogtailMonitor instance;
    return &instance;
}

bool LogtailMonitor::Init() {
    mScaledCpuUsageUpLimit = AppConfig::GetInstance()->GetCpuUsageUpLimit();
    mStatusCount = 0;

    // Reset process and realtime CPU statistics.
    mCpuStat.Reset();
    mRealtimeCpuStat.Reset();
    // Reset memory statistics.
    mMemStat.Reset();

#if defined(__linux__)
    // Reset OS CPU statistics.
    CalCpuCores();
    mScaledCpuUsageStep = 0.1;
    mOsCpuStatForScale.Reset();
    CalOsCpuStat();
    for (int32_t i = 0; i < CPU_STAT_FOR_SCALE_ARRAY_SIZE; ++i) {
        mCpuArrayForScale[i] = 0;
        mOsCpuArrayForScale[i] = mOsCpuStatForScale.mOsCpuUsage;
    }
    mCpuArrayForScaleIdx = 0;
#endif

    // init metrics
    mGlobalCpuGauge = LoongCollectorMonitor::GetInstance()->GetDoubleGauge(METRIC_AGENT_CPU);
    mGlobalMemoryGauge = LoongCollectorMonitor::GetInstance()->GetIntGauge(METRIC_AGENT_MEMORY);
    mGlobalUsedSendingConcurrency
        = LoongCollectorMonitor::GetInstance()->GetIntGauge(METRIC_AGENT_USED_SENDING_CONCURRENCY);

    // Initialize monitor thread.
    mThreadRes = async(launch::async, &LogtailMonitor::Monitor, this);
    return true;
}

void LogtailMonitor::Stop() {
    {
        lock_guard<mutex> lock(mThreadRunningMux);
        mIsThreadRunning = false;
    }
    mStopCV.notify_one();
    future_status s = mThreadRes.wait_for(chrono::seconds(1));
    if (s == future_status::ready) {
        LOG_INFO(sLogger, ("profiling", "stopped successfully"));
    } else {
        LOG_WARNING(sLogger, ("profiling", "forced to stopped"));
    }
}

void LogtailMonitor::Monitor() {
    LOG_INFO(sLogger, ("profiling", "started"));
    int32_t lastMonitorTime = time(NULL), lastCheckHardLimitTime = time(nullptr);
    CpuStat curCpuStat;
    {
        unique_lock<mutex> lock(mThreadRunningMux);
        while (mIsThreadRunning) {
            if (mStopCV.wait_for(lock, std::chrono::seconds(1), [this]() { return !mIsThreadRunning; })) {
                break;
            }
            GetCpuStat(curCpuStat);

            // Update mRealtimeCpuStat for InputFlowControl.
            if (AppConfig::GetInstance()->IsInputFlowControl()) {
                CalCpuStat(curCpuStat, mRealtimeCpuStat);
            }

            int32_t monitorTime = time(NULL);
#if defined(__linux__) // TODO: Add auto scale support for Windows.
            // Update related CPU statistics for controlling resource auto scale (Linux only).
            if (AppConfig::GetInstance()->IsResourceAutoScale()) {
                CalCpuStat(curCpuStat, mCpuStatForScale);
                CalOsCpuStat();
                mCpuArrayForScale[mCpuArrayForScaleIdx % CPU_STAT_FOR_SCALE_ARRAY_SIZE] = mCpuStatForScale.mCpuUsage;
                mOsCpuArrayForScale[mCpuArrayForScaleIdx % CPU_STAT_FOR_SCALE_ARRAY_SIZE]
                    = mOsCpuStatForScale.mOsCpuUsage;
                ++mCpuArrayForScaleIdx;
                CheckScaledCpuUsageUpLimit();
                LOG_DEBUG(sLogger,
                          ("mCpuStatForScale", mCpuStatForScale.mCpuUsage)("mOsCpuStatForScale",
                                                                           mOsCpuStatForScale.mOsCpuUsage));
            }
#endif

            static int32_t checkHardLimitInterval
                = INT32_FLAG(monitor_interval) > 30 ? INT32_FLAG(monitor_interval) / 6 : 5;
            if ((monitorTime - lastCheckHardLimitTime) >= checkHardLimitInterval) {
                lastCheckHardLimitTime = monitorTime;

                GetMemStat();
                if (CheckHardMemLimit()) {
                    LOG_ERROR(sLogger,
                              ("Resource used by program exceeds hard limit",
                               "prepare restart Logtail")("mem_rss", mMemStat.mRss));
                    Suicide();
                }
            }


            // Update statistics and send to logtail_status_profile regularly.
            // If CPU or memory limit triggered, send to logtail_suicide_profile.
            if ((monitorTime - lastMonitorTime) >= INT32_FLAG(monitor_interval)) {
                lastMonitorTime = monitorTime;

                // Memory usage has exceeded limit, try to free some timeout objects.
                if (1 == mMemStat.mViolateNum) {
                    LOG_DEBUG(sLogger, ("Memory is upper limit", "run gabbage collection."));
                    LogInput::GetInstance()->SetForceClearFlag(true);
                }
                // CalCpuLimit and CalMemLimit will check if the number of violation (CPU
                // or memory exceeds limit) // is greater or equal than limits (
                // flag(cpu_limit_num) and flag(mem_limit_num)).
                // Returning true means too much violations, so we have to prepare to restart
                // logtail to release resource.
                // Mainly for controlling memory because we have no idea to descrease memory usage.
                if (CheckSoftCpuLimit() || CheckSoftMemLimit()) {
                    LOG_ERROR(sLogger,
                              ("Resource used by program exceeds upper limit for some time",
                               "prepare restart Logtail")("cpu_usage", mCpuStat.mCpuUsage)("mem_rss", mMemStat.mRss));
                    Suicide();
                }

                if (IsHostIpChanged()) {
                    Suicide();
                }

                SendStatusProfile(false);
                if (BOOL_FLAG(logtail_dump_monitor_info)) {
                    if (!DumpMonitorInfo(monitorTime))
                        LOG_ERROR(sLogger, ("Fail to dump monitor info", ""));
                }
            }
        }
    }
    SendStatusProfile(true);
}

template <typename T>
static void AddLogContent(sls_logs::Log* log, const char* key, const T& val) {
    auto content = log->add_contents();
    content->set_key(key);
    content->set_value(ToString(val));
}

bool LogtailMonitor::SendStatusProfile(bool suicide) {
    mStatusCount++;
    string category;
    if (suicide)
        category = "logtail_suicide_profile";
    else if (mStatusCount % 2 == 0)
        category = "logtail_status_profile";
    else
        return false;

    auto now = GetCurrentLogtailTime();
    // Check input thread.
    int32_t lastReadEventTime = LogInput::GetInstance()->GetLastReadEventTime();
    if (lastReadEventTime > 0
        && (now.tv_sec - lastReadEventTime > AppConfig::GetInstance()->GetForceQuitReadTimeout())) {
        LOG_ERROR(sLogger, ("last read event time is too old", lastReadEventTime)("prepare force exit", ""));
        LogtailAlarm::GetInstance()->SendAlarm(
            LOGTAIL_CRASH_ALARM, "last read event time is too old: " + ToString(lastReadEventTime) + " force exit");
        LogtailAlarm::GetInstance()->ForceToSend();
        sleep(10);
        _exit(1);
    }

    // the unique id of current instance
    std::string id = sdk::Base64Enconde(LogFileProfiler::mHostname + LogFileProfiler::mIpAddr + ILOGTAIL_VERSION
                                        + GetProcessExecutionDir());

    // Collect status information to send.
    LogGroup logGroup;
    logGroup.set_category(category);
    logGroup.set_source(LogFileProfiler::mIpAddr);
    Log* logPtr = logGroup.add_logs();
    SetLogTime(logPtr, AppConfig::GetInstance()->EnableLogTimeAutoAdjust() ? now.tv_sec + GetTimeDelta() : now.tv_sec);
    // CPU usage of Logtail process.
    AddLogContent(logPtr, "cpu", mCpuStat.mCpuUsage);
    mGlobalCpuGauge->Set(mCpuStat.mCpuUsage);
#if defined(__linux__) // TODO: Remove this if auto scale is available on Windows.
    // CPU usage of system.
    AddLogContent(logPtr, "os_cpu", mOsCpuStatForScale.mOsCpuUsage);
#endif
    // Memory usage of Logtail process.
    AddLogContent(logPtr, "mem", mMemStat.mRss);
    mGlobalMemoryGauge->Set(mMemStat.mRss);
    // The version, uuid of Logtail.
    AddLogContent(logPtr, "version", ILOGTAIL_VERSION);
    AddLogContent(logPtr, "uuid", Application::GetInstance()->GetUUID());
#ifdef __ENTERPRISE__
    AddLogContent(logPtr, "user_defined_id", EnterpriseConfigProvider::GetInstance()->GetUserDefinedIdSet());
    AddLogContent(logPtr, "aliuids", EnterpriseConfigProvider::GetInstance()->GetAliuidSet());
#endif
    AddLogContent(logPtr, "projects", FlusherSLS::GetAllProjects());
    AddLogContent(logPtr, "instance_id", Application::GetInstance()->GetInstanceId());
    AddLogContent(logPtr, "instance_key", id);
    AddLogContent(logPtr, "syslog_open", AppConfig::GetInstance()->GetOpenStreamLog());
    // Host informations.
    AddLogContent(logPtr, "ip", LogFileProfiler::mIpAddr);
    AddLogContent(logPtr, "hostname", LogFileProfiler::mHostname);
    AddLogContent(logPtr, "os", OS_NAME);
    AddLogContent(logPtr, "os_detail", LogFileProfiler::mOsDetail);
    AddLogContent(logPtr, "user", LogFileProfiler::mUsername);
#if defined(__linux__)
    AddLogContent(logPtr, "load", GetLoadAvg());
#endif
    AddLogContent(logPtr, "plugin_stats", PipelineManager::GetInstance()->GetPluginStatistics());
    // Metrics.
    vector<string> allProfileRegion;
    ProfileSender::GetInstance()->GetAllProfileRegion(allProfileRegion);
    UpdateMetric("region", allProfileRegion);
#ifdef __ENTERPRISE__
    UpdateMetric("config_update_count", EnterpriseConfigProvider::GetInstance()->GetConfigUpdateTotalCount());
    UpdateMetric("config_update_item_count", EnterpriseConfigProvider::GetInstance()->GetConfigUpdateItemTotalCount());
    UpdateMetric("config_update_last_time",
                 GetTimeStamp(EnterpriseConfigProvider::GetInstance()->GetLastConfigUpdateTime(), "%Y-%m-%d %H:%M:%S"));
    UpdateMetric("config_get_last_time",
                 GetTimeStamp(EnterpriseConfigProvider::GetInstance()->GetLastConfigGetTime(), "%Y-%m-%d %H:%M:%S"));
#endif
    UpdateMetric("config_prefer_real_ip", BOOL_FLAG(send_prefer_real_ip));
    UpdateMetric("plugin_enabled", LogtailPlugin::GetInstance()->IsPluginOpened());
#if defined(__linux__) && !defined(__ANDROID__)
    UpdateMetric("observer_enabled", ObserverManager::GetInstance()->Status());
#endif
    const std::vector<sls_logs::LogTag>& envTags = AppConfig::GetInstance()->GetEnvTags();
    if (!envTags.empty()) {
        UpdateMetric("env_config_count", envTags.size());
    }
    int32_t usedSendingConcurrency = FlusherRunner::GetInstance()->GetSendingBufferCount();
    UpdateMetric("used_sending_concurrency", usedSendingConcurrency);
    mGlobalUsedSendingConcurrency->Set(usedSendingConcurrency);

    AddLogContent(logPtr, "metric_json", MetricToString());
    AddLogContent(logPtr, "status", CheckLogtailStatus());
    AddLogContent(logPtr, "ecs_instance_id", LogFileProfiler::mECSInstanceID);
    AddLogContent(logPtr, "ecs_user_id", LogFileProfiler::mECSUserID);
    AddLogContent(logPtr, "ecs_regioon_id", LogFileProfiler::mECSRegionID);
    ClearMetric();

    if (!mIsThreadRunning)
        return false;

    // Dump to local and send to enabled regions.
    DumpToLocal(logGroup);
    for (size_t i = 0; i < allProfileRegion.size(); ++i) {
        if (BOOL_FLAG(check_profile_region) && !FlusherSLS::IsRegionContainingConfig(allProfileRegion[i])) {
            LOG_DEBUG(sLogger, ("region does not contain config for this instance", allProfileRegion[i]));
            continue;
        }

        // Check if the region is disabled.
        if (!FlusherSLS::GetRegionStatus(allProfileRegion[i])) {
            LOG_DEBUG(sLogger, ("disabled region, do not send status profile to region", allProfileRegion[i]));
            continue;
        }

        if (i == allProfileRegion.size() - 1) {
            ProfileSender::GetInstance()->SendToProfileProject(allProfileRegion[i], logGroup);
        } else {
            LogGroup copyLogGroup = logGroup;
            ProfileSender::GetInstance()->SendToProfileProject(allProfileRegion[i], copyLogGroup);
        }
    }
    return true;
}

bool LogtailMonitor::GetMemStat() {
#if defined(__linux__)
    const char* SELF_STATM_PATH = "/proc/self/statm";

    std::ifstream fin;
    fin.open(SELF_STATM_PATH);
    if (!fin.good()) {
        LOG_ERROR(sLogger, ("open stat error", ""));
        return false;
    }
    fin.ignore(100, ' ');
    fin >> mMemStat.mRss;
    uint32_t pagesize = getpagesize();
    pagesize /= 1024; // page size in kb
    mMemStat.mRss *= pagesize;
    mMemStat.mRss /= 1024; // rss in mb
    fin.close();
    return true;
#elif defined(_MSC_VER)
    PROCESS_MEMORY_COUNTERS pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    mMemStat.mRss = pmc.WorkingSetSize / 1024 / 1024;
    return true;
#endif
}

// Linux: get from /proc/self/stat.
// Windows: call GetSystemTimeAsFileTime to get global CPU time and
//   call GetProcessTimes to get CPU times of logtail process.
// NOTE: for Linux, single core CPU usage is returned, but for Windows,
//   it's whole CPU usage.
bool LogtailMonitor::GetCpuStat(CpuStat& cur) {
#if defined(__linux__)
    const char* SELF_STAT_PATH = "/proc/self/stat";

    std::ifstream fin;
    fin.open(SELF_STAT_PATH);
    uint64_t start = GetCurrentTimeInMilliSeconds();
    if (!fin.good()) {
        LOG_ERROR(sLogger, ("open stat error", ""));
        return false;
    }
    for (uint32_t i = 0; i < 13; ++i) {
        fin.ignore(100, ' ');
    }
    fin >> cur.mUserTime;
    fin >> cur.mSysTime;
    fin.close();
    uint64_t end = GetCurrentTimeInMilliSeconds();
    cur.mSysTotalTime = (start + end) / 2;
    return true;
#elif defined(_MSC_VER)
    FILETIME ftime, fsys, fuser;
    ULARGE_INTEGER now, sys, user;
    GetSystemTimeAsFileTime(&ftime);
    memcpy(&now, &ftime, sizeof(FILETIME));
    GetProcessTimes(cur.mSelf, &ftime, &ftime, &fsys, &fuser);
    memcpy(&sys, &fsys, sizeof(FILETIME));
    memcpy(&user, &fuser, sizeof(FILETIME));
    float percent = (sys.QuadPart - cur.mLastSysCPU.QuadPart) + (user.QuadPart - cur.mLastUserCPU.QuadPart);
    percent /= (now.QuadPart - cur.mLastCPU.QuadPart);
    // percent /= mCPUStat.mNumProcessors;//compute single core cpu util, as Linux logtail
    cur.mLastCPU = now;
    cur.mLastUserCPU = user;
    cur.mLastSysCPU = sys;
    cur.mCpuUsage = percent;
    return true;
#endif
}

void LogtailMonitor::CalCpuStat(const CpuStat& curCpu, CpuStat& savedCpu) {
#if defined(__linux__)
    const float MILLI_TICK_PER_SEC = 1000.0;

    int64_t delta = curCpu.mSysTotalTime - savedCpu.mSysTotalTime;
    if (delta == 0) {
        return;
    }
    savedCpu.mCpuUsage = (curCpu.mUserTime + curCpu.mSysTime - savedCpu.mUserTime - savedCpu.mSysTime) * 1.0 / HZ
        / (delta / MILLI_TICK_PER_SEC);
    savedCpu.mUserTime = curCpu.mUserTime;
    savedCpu.mSysTime = curCpu.mSysTime;
    savedCpu.mSysTotalTime = curCpu.mSysTotalTime;
#elif defined(_MSC_VER)
    float percent = (curCpu.mLastSysCPU.QuadPart - savedCpu.mLastSysCPU.QuadPart)
        + (curCpu.mLastUserCPU.QuadPart - savedCpu.mLastUserCPU.QuadPart);
    percent /= (curCpu.mLastCPU.QuadPart - savedCpu.mLastCPU.QuadPart);
    savedCpu.mCpuUsage = percent;
    savedCpu.mLastCPU = curCpu.mLastCPU;
    savedCpu.mLastSysCPU = curCpu.mLastSysCPU;
    savedCpu.mLastUserCPU = curCpu.mLastUserCPU;
#endif
}

bool LogtailMonitor::CheckSoftCpuLimit() {
    float cpuUsageLimit = AppConfig::GetInstance()->IsResourceAutoScale()
        ? AppConfig::GetInstance()->GetScaledCpuUsageUpLimit()
        : AppConfig::GetInstance()->GetCpuUsageUpLimit();
    if (cpuUsageLimit < mCpuStat.mCpuUsage) {
        if (++mCpuStat.mViolateNum > INT32_FLAG(cpu_limit_num))
            return true;
    } else
        mCpuStat.mViolateNum = 0;
    return false;
}

bool LogtailMonitor::CheckSoftMemLimit() {
    if (mMemStat.mRss > AppConfig::GetInstance()->GetMemUsageUpLimit()) {
        if (++mMemStat.mViolateNum > INT32_FLAG(mem_limit_num))
            return true;
    } else
        mMemStat.mViolateNum = 0;
    return false;
}

bool LogtailMonitor::CheckHardMemLimit() {
    return mMemStat.mRss > 5 * AppConfig::GetInstance()->GetMemUsageUpLimit();
}

void LogtailMonitor::DumpToLocal(const sls_logs::LogGroup& logGroup) {
    string dumpStr = "\n####logtail status####\n";
    for (int32_t logIdx = 0; logIdx < logGroup.logs_size(); ++logIdx) {
        Json::Value category;
        const Log& log = logGroup.logs(logIdx);
        for (int32_t conIdx = 0; conIdx < log.contents_size(); ++conIdx) {
            const Log_Content& content = log.contents(conIdx);
            const string& key = content.key();
            const string& value = content.value();
            dumpStr.append(key).append(":").append(value).append("\n");
        }
    }
    dumpStr += "####status     end####\n";

    static auto gMonitorLogger = Logger::Instance().GetLogger("/apsara/sls/ilogtail/status");
    LOG_INFO(gMonitorLogger, ("\n", dumpStr));
}

bool LogtailMonitor::DumpMonitorInfo(time_t monitorTime) {
    string path = GetProcessExecutionDir() + "logtail_monitor_info";
    ofstream outfile(path.c_str(), ofstream::app);
    if (!outfile)
        return false;
    outfile << "time:" << monitorTime << "\t";
    outfile << "cpu_usage:" << mCpuStat.mCpuUsage << "\t";
    outfile << "mem_rss:" << mMemStat.mRss << "\n";
    return true;
}

bool LogtailMonitor::IsHostIpChanged() {
    if (AppConfig::GetInstance()->GetConfigIP().empty()) {
        const std::string& interface = AppConfig::GetInstance()->GetBindInterface();
        std::string ip = GetHostIp();
        if (interface.size() > 0) {
            ip = GetHostIp(interface);
        }
        if (ip.empty()) {
            ip = GetAnyAvailableIP();
        }
        if (ip != LogFileProfiler::mIpAddr) {
            LOG_ERROR(sLogger,
                      ("error", "host ip changed during running, prepare to restart Logtail")(
                          "original ip", LogFileProfiler::mIpAddr)("current ip", ip));
            return true;
        }
        return false;
    }
    return false;
}

void LogtailMonitor::Suicide() {
    SendStatusProfile(true);
    mIsThreadRunning = false;
    Application::GetInstance()->SetSigTermSignalFlag(true);
    sleep(60);
    _exit(1);
}

#if defined(__linux__) // Linux only methods, for scale up calculation, load average.
static const char* PROC_STAT_PATH = "/proc/stat";

std::string LogtailMonitor::GetLoadAvg() {
    const char* PROC_LOAD_PATH = "/proc/loadavg";

    std::ifstream fin;
    std::string loadStr;
    fin.open(PROC_LOAD_PATH);
    if (!fin.good()) {
        LOG_ERROR(sLogger, ("open load error", ""));
        return loadStr;
    }
    std::getline(fin, loadStr);
    fin.close();
    return loadStr;
}

uint32_t LogtailMonitor::GetCpuCores() {
    if (!CalCpuCores()) {
        return 0;
    }
    return mCpuCores;
}

// Get the number of cores in CPU.
bool LogtailMonitor::CalCpuCores() {
    ifstream fin;
    fin.open(PROC_STAT_PATH);
    if (fin.fail()) {
        LOG_ERROR(sLogger, ("get count of cpu cores fail, can't open file", PROC_STAT_PATH));
        mCpuCores = 1;
        return false;
    }

    char buf[2048];
    string id;

    mCpuCores = 0;
    while (true) {
        fin >> id;
        fin.getline(buf, 2048);
        if (id.find("cpu") != 0)
            break;
        if (id != "cpu")
            ++mCpuCores;
        if (fin.eof())
            break;
    }
    fin.close();
    if (mCpuCores == 0) {
        LOG_ERROR(sLogger, ("get count of cpu cores fail, can't parse file", PROC_STAT_PATH));
        mCpuCores = 1;
        return false;
    }
    LOG_INFO(sLogger, ("machine cpu cores", mCpuCores));
    return true;
}

// Use mCpuArrayForScale and mOsCpuArrayForScale to calculate if ilogtail can scale up
// to use more CPU or scale down.
void LogtailMonitor::CheckScaledCpuUsageUpLimit() {
    // flag(cpu_usage_up_limit) or cpu_usage_limit in ilogtail_config.json.
    float cpuUsageUpLimit = AppConfig::GetInstance()->GetCpuUsageUpLimit();
    // flag(machine_cpu_usage_threshold) or same name in ilogtail_config.json.
    float machineCpuUsageThreshold = AppConfig::GetInstance()->GetMachineCpuUsageThreshold();
    // mScaledCpuUsageUpLimit is greater or equal than cpuUsageUpLimit.
    // It will be increased when Monitor finds the global CPU usage is low, which means
    // Logtail can use more CPU.
    // It will be descreasd when Monitor finds the global CPU usage is greater than
    // machineCpuUsageThreshold specified.
    // mScaledCpuUsageStep is used to control step for increasing and descreasing,
    // 0.1 by default.
    if (mOsCpuStatForScale.mOsCpuUsage >= machineCpuUsageThreshold) {
        if ((mScaledCpuUsageUpLimit - mScaledCpuUsageStep) < cpuUsageUpLimit)
            mScaledCpuUsageUpLimit = cpuUsageUpLimit;
        else
            mScaledCpuUsageUpLimit -= mScaledCpuUsageStep;
        LOG_DEBUG(sLogger,
                  ("os cpu usage", mOsCpuStatForScale.mOsCpuUsage)("desc mScaledCpuUsageUpLimit to value",
                                                                   mScaledCpuUsageUpLimit));
        return;
    }

    // If the global CPU usage is less than machineCpuUsageThreshold specifed,
    // we can scale up the mScaledCpuUsageUpLimit.
    // Maximum: mCpuCores * machineCpuUsageThreshold.
    // If both of latest two CPU status (stored in ArrayForScale) can not satisfy,
    // we can not scale up, otherwise, we can increase mScaledCpuUsageUpLimit by
    // mScaledCpuUsageStep.
    if (mCpuArrayForScaleIdx % CPU_STAT_FOR_SCALE_ARRAY_SIZE == 0) {
        if ((mScaledCpuUsageUpLimit + mScaledCpuUsageStep) >= (mCpuCores * machineCpuUsageThreshold))
            return;
        for (int32_t i = 0; i < CPU_STAT_FOR_SCALE_ARRAY_SIZE; ++i) {
            if ((mOsCpuArrayForScale[i] / machineCpuUsageThreshold) >= 0.95
                || (mCpuArrayForScale[i] / mScaledCpuUsageUpLimit) < 0.6)
                return;
        }
        mScaledCpuUsageUpLimit += mScaledCpuUsageStep;
        LOG_DEBUG(sLogger,
                  ("os cpu usage", mOsCpuStatForScale.mOsCpuUsage)("inc mScaledCpuUsageUpLimit to value",
                                                                   mScaledCpuUsageUpLimit));
    }
}

bool LogtailMonitor::CalOsCpuStat() {
#if defined(__linux__)
    ifstream fin;
    fin.open(PROC_STAT_PATH);
    if (fin.fail()) {
        LOG_ERROR(sLogger, ("CalOsCpuStat fail, can't open file", PROC_STAT_PATH));
        mOsCpuStatForScale.mOsCpuUsage = 0.5;
        return false;
    }

    static char buf[2048];
    string id;
    int64_t user, nice, system, idle, iowait, irq, softirq;
    fin >> id;
    fin >> user;
    fin >> nice;
    fin >> system;
    fin >> idle;
    fin >> iowait;
    fin >> irq;
    fin >> softirq;
    fin.getline(buf, 2048);
    fin.close();
    int64_t total = user + nice + system + idle + iowait + irq + softirq;
    int64_t noIdle = user + nice + system + irq + softirq;
    int64_t totalDelta = total - mOsCpuStatForScale.mTotal;
    int64_t noIdleDelta = noIdle - mOsCpuStatForScale.mNoIdle;
    mOsCpuStatForScale.mTotal = total;
    mOsCpuStatForScale.mNoIdle = noIdle;
    LOG_DEBUG(sLogger,
              ("cpu", mOsCpuStatForScale.mOsCpuUsage)("id", id)("user", user)("nice", nice)("system", system)(
                  "idle", idle)("iowait", iowait)("irq", irq)("softirq", softirq));
    if (totalDelta <= 0 || noIdleDelta < 0)
        return false;
    else {
        mOsCpuStatForScale.mOsCpuUsage = 1.0 * noIdleDelta / totalDelta;
        return true;
    }
#elif defined(_MSC_VER)
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        LOG_ERROR(sLogger, ("CalOsCpuStat fail, GetSystemTimes failed", GetLastError()));
        mOsCpuStatForScale.mOsCpuUsage = 0.5;
        return false;
    }
    ULARGE_INTEGER idle, kernel, user;
    memcpy(&idle, &idleTime, sizeof(FILETIME));
    memcpy(&kernel, &kernelTime, sizeof(FILETIME));
    memcpy(&user, &userTime, sizeof(FILETIME));

    if (mOsCpuStatForScale.mNoIdle > 0 && mOsCpuStatForScale.mTotal > 0) {
        mOsCpuStatForScale.mOsCpuUsage = (kernel.QuadPart + user.QuadPart - mOsCpuStatForScale.mNoIdle)
            / (idle.QuadPart + user.QuadPart + kernel.QuadPart - mOsCpuStatForScale.mTotal);
    }
    mOsCpuStatForScale.mNoIdle = kernel.QuadPart + user.QuadPart;
    mOsCpuStatForScale.mTotal = mOsCpuStatForScale.mNoIdle + idle.QuadPart;
    return true;
#endif
}
#endif

LoongCollectorMonitor* LoongCollectorMonitor::GetInstance() {
    static LoongCollectorMonitor instance;
    return &instance;
}

void LoongCollectorMonitor::Init() {
    // create metric record
    MetricLabels labels;
    labels.emplace_back(METRIC_LABEL_INSTANCE_ID, Application::GetInstance()->GetInstanceId());
    labels.emplace_back(METRIC_LABEL_IP, LogFileProfiler::mIpAddr);
    labels.emplace_back(METRIC_LABEL_OS, OS_NAME);
    labels.emplace_back(METRIC_LABEL_OS_DETAIL, LogFileProfiler::mOsDetail);
    labels.emplace_back(METRIC_LABEL_UUID, Application::GetInstance()->GetUUID());
    labels.emplace_back(METRIC_LABEL_VERSION, ILOGTAIL_VERSION);
    DynamicMetricLabels dynamicLabels;
    dynamicLabels.emplace_back(METRIC_LABEL_PROJECTS, []() -> std::string { return FlusherSLS::GetAllProjects(); });
#ifdef __ENTERPRISE__
    dynamicLabels.emplace_back(METRIC_LABEL_ALIUIDS,
                               []() -> std::string { return EnterpriseConfigProvider::GetInstance()->GetAliuidSet(); });
    dynamicLabels.emplace_back(METRIC_LABEL_USER_DEFINED_ID, []() -> std::string {
        return EnterpriseConfigProvider::GetInstance()->GetUserDefinedIdSet();
    });
#endif
    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
        mMetricsRecordRef, std::move(labels), std::move(dynamicLabels));
    // init value
    mDoubleGauges[METRIC_AGENT_CPU] = mMetricsRecordRef.CreateDoubleGauge(METRIC_AGENT_CPU);
    // mDoubleGauges[METRIC_AGENT_CPU_GO] = mMetricsRecordRef.CreateDoubleGauge(METRIC_AGENT_CPU_GO);
    mIntGauges[METRIC_AGENT_MEMORY] = mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_MEMORY);
    mIntGauges[METRIC_AGENT_MEMORY_GO] = mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_MEMORY_GO);
    mIntGauges[METRIC_AGENT_OPEN_FD_TOTAL] = mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_OPEN_FD_TOTAL);
    mIntGauges[METRIC_AGENT_POLLING_DIR_CACHE_SIZE_TOTAL]
        = mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_POLLING_DIR_CACHE_SIZE_TOTAL);
    mIntGauges[METRIC_AGENT_POLLING_FILE_CACHE_SIZE_TOTAL]
        = mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_POLLING_FILE_CACHE_SIZE_TOTAL);
    mIntGauges[METRIC_AGENT_POLLING_MODIFY_SIZE_TOTAL]
        = mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_POLLING_MODIFY_SIZE_TOTAL);
    mIntGauges[METRIC_AGENT_REGISTER_HANDLER_TOTAL]
        = mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_REGISTER_HANDLER_TOTAL);
    // mIntGauges[METRIC_AGENT_INSTANCE_CONFIG_TOTAL] =
    // mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_INSTANCE_CONFIG_TOTAL);
    mIntGauges[METRIC_AGENT_PIPELINE_CONFIG_TOTAL]
        = mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_PIPELINE_CONFIG_TOTAL);
    // mIntGauges[METRIC_AGENT_ENV_PIPELINE_CONFIG_TOTAL] =
    // mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_ENV_PIPELINE_CONFIG_TOTAL);
    // mIntGauges[METRIC_AGENT_CRD_PIPELINE_CONFIG_TOTAL] =
    // mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_CRD_PIPELINE_CONFIG_TOTAL);
    // mIntGauges[METRIC_AGENT_CONSOLE_PIPELINE_CONFIG_TOTAL]
    //     = mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_CONSOLE_PIPELINE_CONFIG_TOTAL);
    // mIntGauges[METRIC_AGENT_PLUGIN_TOTAL] = mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_PLUGIN_TOTAL);
    mIntGauges[METRIC_AGENT_PROCESS_QUEUE_FULL_TOTAL]
        = mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_PROCESS_QUEUE_FULL_TOTAL);
    mIntGauges[METRIC_AGENT_PROCESS_QUEUE_TOTAL] = mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_PROCESS_QUEUE_TOTAL);
    mIntGauges[METRIC_AGENT_SEND_QUEUE_FULL_TOTAL]
        = mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_SEND_QUEUE_FULL_TOTAL);
    mIntGauges[METRIC_AGENT_SEND_QUEUE_TOTAL] = mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_SEND_QUEUE_TOTAL);
    mIntGauges[METRIC_AGENT_USED_SENDING_CONCURRENCY]
        = mMetricsRecordRef.CreateIntGauge(METRIC_AGENT_USED_SENDING_CONCURRENCY);
    LOG_INFO(sLogger, ("LoongCollectorMonitor", "started"));
}

void LoongCollectorMonitor::Stop() {
}

CounterPtr LoongCollectorMonitor::GetCounter(std::string key) {
    auto it = mCounters.find(key);
    if (it != mCounters.end()) {
        return it->second;
    }
    LOG_WARNING(sLogger, ("get global counter failed, counter key", key));
    return nullptr;
}

IntGaugePtr LoongCollectorMonitor::GetIntGauge(std::string key) {
    auto it = mIntGauges.find(key);
    if (it != mIntGauges.end()) {
        return it->second;
    }
    LOG_WARNING(sLogger, ("get global gauge failed, gauge key", key));
    return nullptr;
}

DoubleGaugePtr LoongCollectorMonitor::GetDoubleGauge(std::string key) {
    auto it = mDoubleGauges.find(key);
    if (it != mDoubleGauges.end()) {
        return it->second;
    }
    LOG_WARNING(sLogger, ("get global gauge failed, gauge key", key));
    return nullptr;
}

} // namespace logtail
