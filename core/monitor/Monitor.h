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

#include <condition_variable>
#include <future>
#include <mutex>
#include <string>
#include <unordered_map>

#include "MetricManager.h"
#include "MetricTypes.h"

#if defined(_MSC_VER)
#include <Windows.h>
#endif

namespace sls_logs {
class LogGroup;
}

namespace logtail {
#ifndef LOGTAIL_NO_TC_MALLOC
extern time_t gLastTcmallocReleaseMemTime;
#endif

struct CpuStat {
#if defined(__linux__)
    uint64_t mSysTotalTime;
    uint64_t mSysTime;
    uint64_t mUserTime;
#elif defined(_MSC_VER)
    ULARGE_INTEGER mLastCPU;
    ULARGE_INTEGER mLastSysCPU;
    ULARGE_INTEGER mLastUserCPU;
    int mNumProcessors;
    HANDLE mSelf;
#endif

    // Common info.
    int32_t mViolateNum;
    float mCpuUsage;

    CpuStat() { Reset(); }
    void Reset();
};

struct MemStat {
    int64_t mRss;
    int32_t mViolateNum;

    void Reset() {
        mRss = 0;
        mViolateNum = 0;
    }
};

struct OsCpuStat {
    int64_t mNoIdle;
    int64_t mTotal;
    float mOsCpuUsage;

    void Reset() {
        mNoIdle = 0;
        mTotal = 0;
        mOsCpuUsage = 0;
    }
};

class LogtailMonitor {
public:
    LogtailMonitor(const LogtailMonitor&) = delete;
    LogtailMonitor& operator=(const LogtailMonitor&) = delete;

    static LogtailMonitor* GetInstance();

    bool Init();
    void Stop();

    uint32_t GetCpuCores();

    // GetRealtimeCpuLevel return a value to indicates current CPU usage level.
    // LogInput use it to do flow control.
    float GetRealtimeCpuLevel() { return mRealtimeCpuStat.mCpuUsage / mScaledCpuUsageUpLimit; }

private:
    LogtailMonitor();
    ~LogtailMonitor() = default;

    void Monitor();
    // GetCpuStat gets current CPU statistics of Logtail process and save it to @cpuStat.
    // @return true if get successfully.
    bool GetCpuStat(CpuStat& cpuStat);
    // GetMemStat gets current memory statistics of Logtail process (save to mMemStat).
    bool GetMemStat();

    // CalCpuStat calculates CPU usage use (@curCpu - @savedCpu) and
    // set @curCpu to @savedCpu after calculation.
    void CalCpuStat(const CpuStat& curCpu, CpuStat& savedCpu);

    // CheckSoftCpuLimit checks if current cpu usage exceeds limit.
    // @return true if the cpu usage exceeds limit continuously.
    bool CheckSoftCpuLimit();
    // CheckSoftMemLimit checks if the memory usage exceeds limit.
    // @return true if the memory usage exceeds limit continuously.
    bool CheckSoftMemLimit();

    bool CheckHardMemLimit();

    // SendStatusProfile collects status profile and send them to server.
    // @suicide indicates if the target LogStore is logtail_suicide_profile.
    //   Because sending is an asynchronous procedure, the caller should wait for
    //   several seconds after calling this method and before _exit(1).
    bool SendStatusProfile(bool suicide);

    // DumpMonitorInfo dumps simple monitor information to local.
    bool DumpMonitorInfo(time_t monitorTime);

#if defined(__linux__)
    // GetLoadAvg gets system load information.
    std::string GetLoadAvg();

    // CalCpuCores calculates the number of cores in CPU.
    bool CalCpuCores();

    // CalOsCpuStat calculates system CPU usage and save it into @mOsCpuStatForScale.
    bool CalOsCpuStat();

    // CheckScaledCpuUsageUpLimit updates mScaledCpuUsageUpLimit according to current
    // status and limits in configurations, so that Logtail can adjust its CPU usage.
    void CheckScaledCpuUsageUpLimit();
#endif

    // IsHostIpChanged checks if the host ip is changed during running.
    bool IsHostIpChanged();

    void Suicide();

    std::future<void> mThreadRes;
    std::mutex mThreadRunningMux;
    bool mIsThreadRunning = true;
    std::atomic_bool mShouldSuicide;
    std::condition_variable mStopCV;

    // Control report status profile frequency.
    int32_t mStatusCount;

    // Use to calculate realtime CPU level (updated every 1s).
    CpuStat mRealtimeCpuStat;
    // Use to calculate CPU limit, updated regularly (30s by default).
    CpuStat mCpuStat;
    // Memory usage statistics.
    MemStat mMemStat;

    // Current scale up level, updated by CheckScaledCpuUsageUpLimit.
    float mScaledCpuUsageUpLimit;
#if defined(__linux__)
    const static int32_t CPU_STAT_FOR_SCALE_ARRAY_SIZE = 2;
    int32_t mCpuCores = 0;
    CpuStat mCpuStatForScale;
    OsCpuStat mOsCpuStatForScale;
    // mCpuArrayForScale and mOsCpuArrayForScale store lastest two CPU usage of
    // ilogtail process and global.
    float mCpuArrayForScale[CPU_STAT_FOR_SCALE_ARRAY_SIZE];
    float mOsCpuArrayForScale[CPU_STAT_FOR_SCALE_ARRAY_SIZE];
    int32_t mCpuArrayForScaleIdx;
    float mScaledCpuUsageStep;
#endif

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ConfigUpdatorUnittest;
#endif
};

class LoongCollectorMonitor {
public:
    static LoongCollectorMonitor* GetInstance();

    void Init();
    void Stop();

    bool GetAgentMetric(SelfMonitorMetricEvent& event);
    void SetAgentMetric(const SelfMonitorMetricEvent& event);
    bool GetRunnerMetric(const std::string& runnerName, SelfMonitorMetricEvent& event);
    void SetRunnerMetric(const std::string& runnerName, const SelfMonitorMetricEvent& event);

    void SetAgentCpu(double cpu) { SET_GAUGE(mAgentCpu, cpu); }
    void SetAgentMemory(uint64_t mem) { SET_GAUGE(mAgentMemory, mem); }
    void SetAgentGoMemory(uint64_t mem) { SET_GAUGE(mAgentGoMemory, mem); }
    void SetAgentGoRoutinesTotal(uint64_t total) { SET_GAUGE(mAgentGoRoutinesTotal, total); }
    void SetAgentOpenFdTotal(uint64_t total) {
#ifndef APSARA_UNIT_TEST_MAIN
        SET_GAUGE(mAgentOpenFdTotal, total);
#endif
    }
    void SetAgentConfigTotal(uint64_t total) {
#ifndef APSARA_UNIT_TEST_MAIN
        SET_GAUGE(mAgentConfigTotal, total);
#endif
    }
    void SetAgentHostMonitorTotal(uint64_t total) {
#ifndef APSARA_UNIT_TEST_MAIN
        SET_GAUGE(mAgentHostMonitorTotal, total);
#endif
    }

    static std::string mHostname;
    static std::string mIpAddr;
    static std::string mOsDetail;
    static std::string mUsername;
    static int32_t mSystemBootTime;
    static std::string mStartTime;

private:
    LoongCollectorMonitor();
    ~LoongCollectorMonitor();

    // 一个全局级别指标的副本，由 SelfMonitorServer::PushSelfMonitorMetricEvents 更新，格式为：
    // {MetricCategory: {key:MetricValue}}
    // 现支持 Agent 和 Runner 指标的保存、获取
    struct GlobalMetrics {
        SelfMonitorMetricEvent mAgentMetric;
        std::unordered_map<std::string, SelfMonitorMetricEvent> mRunnerMetrics;
    };
    std::mutex mGlobalMetricsMux;
    GlobalMetrics mGlobalMetrics;

    // MetricRecord
    MetricsRecordRef mMetricsRecordRef;

    DoubleGaugePtr mAgentCpu;
    IntGaugePtr mAgentMemory;
    IntGaugePtr mAgentGoMemory;
    IntGaugePtr mAgentGoRoutinesTotal;
    IntGaugePtr mAgentOpenFdTotal;
    IntGaugePtr mAgentConfigTotal;
    IntGaugePtr mAgentHostMonitorTotal;
};

} // namespace logtail
