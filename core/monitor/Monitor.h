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
#include "MetricStore.h"
#include <string>
#include "common/Thread.h"
#include "profile_sender/ProfileSender.h"
#if defined(_MSC_VER)
#include <Windows.h>
#endif

namespace sls_logs {
class LogGroup;
}

namespace logtail {

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

class LogtailMonitor : public MetricStore {
public:
    LogtailMonitor();
    ~LogtailMonitor();
    static LogtailMonitor* Instance() {
        static LogtailMonitor* monitorPtr = new LogtailMonitor();
        return monitorPtr;
    }

    bool InitMonitor();
    bool RemoveMonitor();

    // GetRealtimeCpuLevel return a value to indicates current CPU usage level.
    // LogInput use it to do flow control.
    float GetRealtimeCpuLevel() { return mRealtimeCpuStat.mCpuUsage / mScaledCpuUsageUpLimit; }

private:
    ThreadPtr mMonitorThreadPtr;
    void Monitor();
    volatile bool mMonitorRunning;

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
    int32_t mCpuCores;
    CpuStat mCpuStatForScale;
    OsCpuStat mOsCpuStatForScale;
    // mCpuArrayForScale and mOsCpuArrayForScale store lastest two CPU usage of
    // ilogtail process and global.
    float mCpuArrayForScale[CPU_STAT_FOR_SCALE_ARRAY_SIZE];
    float mOsCpuArrayForScale[CPU_STAT_FOR_SCALE_ARRAY_SIZE];
    int32_t mCpuArrayForScaleIdx;
    float mScaledCpuUsageStep;
#endif
    ProfileSender mProfileSender;

private:
    // GetCpuStat gets current CPU statistics of Logtail process and save it to @cpuStat.
    // @return true if get successfully.
    bool GetCpuStat(CpuStat& cpuStat);
    // GetMemStat gets current memory statistics of Logtail process (save to mMemStat).
    bool GetMemStat();

    // CalCpuStat calculates CPU usage use (@curCpu - @savedCpu) and
    // set @curCpu to @savedCpu after calculation.
    void CalCpuStat(const CpuStat& curCpu, CpuStat& savedCpu);

    // CheckCpuLimit checks if current cpu usage exceeds limit.
    // @return true if the cpu usage exceeds limit continuously.
    bool CheckCpuLimit();
    // CheckMemLimit checks if the memory usage exceeds limit.
    // @return true if the memory usage exceeds limit continuously.
    bool CheckMemLimit();

    // SendStatusProfile collects status profile and send them to server.
    // @suicide indicates if the target LogStore is logtail_suicide_profile.
    //   Because sending is an asynchronous procedure, the caller should wait for
    //   several seconds after calling this method and before _exit(1).
    bool SendStatusProfile(bool suicide);

    // DumpToLocal dumps the @logGroup to local status log.
    void DumpToLocal(const sls_logs::LogGroup& logGroup);

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

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ConfigUpdatorUnittest;
#endif
};

} // namespace logtail
